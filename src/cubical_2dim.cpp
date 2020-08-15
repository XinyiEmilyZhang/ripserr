/*
 This file is an altered form of the Cubical Ripser software created by
 Takeki Sudo and Kazushi Ahara. Details of the original software are below the
 dashed line.
 -Raoul Wadhwa
 -------------------------------------------------------------------------------
 Copyright 2017-2018 Takeki Sudo and Kazushi Ahara.
 This file is part of CubicalRipser_2dim.
 CubicalRipser: C++ system for computation of Cubical persistence pairs
 Copyright 2017-2018 Takeki Sudo and Kazushi Ahara.
 CubicalRipser is free software: you can redistribute it and/or modify it under
 the terms of the GNU Lesser General Public License as published by the
 Free Software Foundation, either version 3 of the License, or (at your option)
 any later version.
 CubicalRipser is deeply depending on 'Ripser', software for Vietoris-Rips
 persitence pairs by Ulrich Bauer, 2015-2016.  We appreciate Ulrich very much.
 We rearrange his codes of Ripser and add some new ideas for optimization on it
 and modify it for calculation of a Cubical filtration.
 This part of CubicalRiper is a calculator of cubical persistence pairs for
 2 dimensional pixel data. The input data format conforms to that of DIPHA.
 See more descriptions in README.
 This program is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 You should have received a copy of the GNU Lesser General Public License along
 with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define FILE_OUTPUT

#include <iostream>
#include <fstream>
#include <queue>
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <Rcpp.h>

using namespace std;

/*****birthday_index*****/
class BirthdayIndex
{
  //member vars
public:
  double birthday;
  int index;
  int dim;

  // default constructor
  BirthdayIndex()
  {
    birthday = 0;
    index = -1;
    dim = 1;
  }

  // individual params constructor
  BirthdayIndex(double _b, int _i, int _d)
  {
    birthday = _b;
    index = _i;
    dim = _d;
  }

  // copy/clone constructor
  BirthdayIndex(const BirthdayIndex& b)
  {
    birthday = b.birthday;
    index = b.index;
    dim = b.dim;
  }

  // copy method
  void copyBirthdayIndex(BirthdayIndex v)
  {
    birthday = v.birthday;
    index = v.index;
    dim = v.dim;
  }

  // getters
  double getBirthday()
  {
    return birthday;
  }
  long getIndex()
  {
    return index;
  }
  int getDimension()
  {
    return dim;
  }
};

struct BirthdayIndexComparator
{
  bool operator()(const BirthdayIndex& o1, const BirthdayIndex& o2) const
  {
    if (o1.birthday == o2.birthday)
    {
      if (o1.index < o2.index)
      {
        return true;
      }
      else
      {
        return false;
      }
    }
    else
    {
      if (o1.birthday > o2.birthday)
      {
        return true;
      }
      else
      {
        return false;
      }
    }
  }
};

struct BirthdayIndexInverseComparator
{
  bool operator()(const BirthdayIndex& o1, const BirthdayIndex& o2) const
  {
    if (o1.birthday == o2.birthday)
    {
      if (o1.index < o2.index)
      {
        return false;
      }
      else
      {
        return true;
      }
    }
    else
    {
      if (o1.birthday > o2.birthday)
      {
        return false;
      }
      else
      {
        return true;
      }
    }
  }
};

/*****dense_cubical_grids*****/
enum file_format
{
  DIPHA,
  PERSEUS
};

class DenseCubicalGrids // file_read
{
public:
  double threshold;
  int dim;
  int ax, ay;
  double dense2[2048][1024];
  file_format format;

  // constructor (w/ file read)
  DenseCubicalGrids(const Rcpp::NumericMatrix& image, double _threshold)
  {
    // set vars
    threshold = _threshold;
    ax = image.nrow();
    ay = image.ncol();

    // assert that dimensions are not too big
    assert(0 < ax && ax < 2000 && 0 < ay && ay < 1000);

    // copy over data from NumericMatrix into DenseCubicalGrids member var
    double dou;
    for (int y = 0; y < ay + 2; y++)
    {
      for (int x = 0; x < ax + 2; x++)
      {
        if (0 < x && x <= ax && 0 < y && y <= ay)
          dense2[x][y] = image(x - 1, y - 1);
        else
          dense2[x][y] = threshold;
      }
    }
  }

  // getter
  double getBirthday(int index, int dim)
  {
    int cx = index & 0x07ff;
    int cy = (index >> 11) & 0x03ff;
    int cm = (index >> 21) & 0xff;

    switch (dim)
    {
    case 0:
      return dense2[cx][cy];
    case 1:
      switch (cm)
      {
      case 0:
        return max(dense2[cx][cy], dense2[cx + 1][cy]);
      default:
        return max(dense2[cx][cy], dense2[cx][cy + 1]);
      }
    case 2:
      return max(max(dense2[cx][cy], dense2[cx + 1][cy]), max(dense2[cx][cy + 1], dense2[cx + 1][cy + 1]));
    }
    return threshold;
  }
};

/*****write_pairs*****/
class WritePairs
{
public:
  int64_t dim;
  double birth;
  double death;

  // constructor
  WritePairs(int64_t _dim, double _birth, double _death)
  {
    dim = _dim;
    birth = _birth;
    death = _death;
  }

  // getters
  int64_t getDimension()
  {
    return dim;
  }
  double getBirth()
  {
    return birth;
  }
  double getDeath()
  {
    return death;
  }
};

/*****columns_to_reduce*****/
class ColumnsToReduce
{
  // member vars
public:
  vector<BirthdayIndex> columns_to_reduce;
  int dim;
  int max_of_index;

  // constructor
  ColumnsToReduce(DenseCubicalGrids* _dcg)
  {
    dim = 0;
    int ax = _dcg->ax;
    int ay = _dcg->ay;
    max_of_index = 2048 * (ay + 2);
    int index;
    double birthday;
    for (int y = ay; y > 0; --y)
    {
      for (int x = ax; x > 0; --x)
      {
        birthday = _dcg->dense2[x][y];
        index = x | (y << 11);
        if (birthday != _dcg -> threshold)
        {
          columns_to_reduce.push_back(BirthdayIndex(birthday, index, 0));
        }
      }
    }
    sort(columns_to_reduce.begin(), columns_to_reduce.end(), BirthdayIndexComparator());
  }

  // getter (length of member vector)
  int size()
  {
    return columns_to_reduce.size();
  }
};

/*****simplex_coboundary_enumerator*****/
class SimplexCoboundaryEnumerator
{
  // member vars
public:
  BirthdayIndex simplex;
  DenseCubicalGrids* dcg;
  int dim;
  double birthtime;
  int ax, ay;
  int cx, cy, cm;
  int count;
  BirthdayIndex nextCoface;
  double threshold;

  // constructor
  SimplexCoboundaryEnumerator()
  {
    nextCoface = BirthdayIndex(0, -1, 1);
  }

  // member methods
  void setSimplexCoboundaryEnumerator(BirthdayIndex _s, DenseCubicalGrids* _dcg)
  {
    simplex = _s;
    dcg = _dcg;
    dim = simplex.dim;
    birthtime = simplex.birthday;
    ax = _dcg->ax;
    ay = _dcg->ay;

    cx = (simplex.index) & 0x07ff;
    cy = (simplex.index >> 11) & 0x03ff;
    cm = (simplex.index >> 21) & 0xff;

    threshold = _dcg->threshold;
    count = 0;
  }
  bool hasNextCoface()
  {
    int index = 0;
    double birthday = 0;
    switch (dim)
    {
    case 0:
      for (int i = count; i < 4; i++)
      {
        switch (i)
        {
        case 0: // y+
          index = (1 << 21) | ((cy) << 11) | (cx);
          birthday = max(birthtime, dcg->dense2[cx  ][cy+1]);
          break;
        case 1: // y-
          index = (1 << 21) | ((cy-1) << 11) | (cx);
          birthday = max(birthtime, dcg->dense2[cx  ][cy-1]);
          break;
        case 2: // x+
          index = (0 << 21) | ((cy) << 11) | (cx);
          birthday = max(birthtime, dcg->dense2[cx+1][cy  ]);
          break;
        case 3: // x-
          index = (0 << 21) | ((cy) << 11) | (cx-1);
          birthday = max(birthtime, dcg->dense2[cx-1][cy  ]);
          break;
        }

        if (birthday != threshold)
        {
          count = i + 1;
          nextCoface = BirthdayIndex(birthday, index, 1);
          return true;
        }
      }
      return false;
    case 1:
      switch (cm)
      {
      case 0:
        if (count == 0) // upper
        {
          count++;
          index = ((cy) << 11) | cx;
          birthday = max(max(birthtime, dcg->dense2[cx][cy + 1]), dcg->dense2[cx + 1][cy + 1]);
          if (birthday != threshold)
          {
            nextCoface = BirthdayIndex(birthday, index, 2);
            return true;
          }
        }
        if (count == 1) // lower
        {
          count++;
          index = ((cy - 1) << 11) | cx;
          birthday = max(max(birthtime, dcg->dense2[cx][cy - 1]), dcg->dense2[cx + 1][cy - 1]);
          if (birthday != threshold)
          {
            nextCoface = BirthdayIndex(birthday, index, 2);
            return true;
          }
        }
        return false;
      case 1:
        if (count == 0) // right
        {
          count ++;
          index = ((cy) << 11) | cx;
          birthday = max(max(birthtime, dcg->dense2[cx + 1][cy]), dcg->dense2[cx + 1][cy + 1]);
          if (birthday != threshold)
          {
            nextCoface = BirthdayIndex(birthday, index, 2);
            return true;
          }
        }
        if (count == 1) //left
        {
          count++;
          index = ((cy) << 11) | (cx - 1);
          birthday = max(max(birthtime, dcg->dense2[cx - 1][cy]), dcg->dense2[cx - 1][cy + 1]);
          if (birthday != threshold)
          {
            nextCoface = BirthdayIndex(birthday, index, 2);
            return true;
          }
        }
        return false;
      }
    }
    return false;
  }

  // getter
  BirthdayIndex getNextCoface()
  {
    return nextCoface;
  }
};

/*****union_find*****/
class UnionFind
{
  // member vars
public:
  int max_of_index;
  vector<int> parent;
  vector<double> birthtime;
  vector<double> time_max;
  DenseCubicalGrids* dcg;

  // constructor
  UnionFind(int moi, DenseCubicalGrids* _dcg) : parent(moi), birthtime(moi), time_max(moi) // Thie "n" is the number of cubes.
  {
    dcg = _dcg;
    max_of_index = moi;

    for (int i = 0; i < moi; ++i)
    {
      parent[i] = i;
      birthtime[i] = dcg->getBirthday(i, 0);
      time_max[i] = dcg->getBirthday(i, 0);
    }
  }

  // member methods
  int find(int x) // Thie "x" is Index.
  {
    int y = x, z = parent[y];
    while (z != y)
    {
      y = z;
      z = parent[y];
    }
    y = parent[x];
    while (z != y)
    {
      parent[x] = z;
      x = y;
      y = parent[x];
    }
    return z;
  }

  void link(int x, int y)
  {
    x = find(x);
    y = find(y);
    if (x == y) return;
    if (birthtime[x] > birthtime[y])
    {
      parent[x] = y;
      birthtime[y] = min(birthtime[x], birthtime[y]);
      time_max[y] = max(time_max[x], time_max[y]);
    }
    else if (birthtime[x] < birthtime[y])
    {
      parent[y] = x;
      birthtime[x] = min(birthtime[x], birthtime[y]);
      time_max[x] = max(time_max[x], time_max[y]);
    }
    else //birthtime[x] == birthtime[y]
    {
      parent[x] = y;
      time_max[y] = max(time_max[x], time_max[y]);
    }
  }
};

/*****joint_pairs*****/
class JointPairs
{
  int n; // the number of cubes
  int ctr_moi;
  int ax, ay;
  DenseCubicalGrids* dcg;
  ColumnsToReduce* ctr;
  vector<WritePairs> *wp;
  bool print;
  double u, v;
  vector<int64_t> cubes_edges;
  vector<BirthdayIndex> dim1_simplex_list;

public:
  // constructor
  JointPairs(DenseCubicalGrids* _dcg, ColumnsToReduce* _ctr, vector<WritePairs> &_wp, const bool _print)
  {
    dcg = _dcg;
    ax = dcg -> ax;
    ay = dcg -> ay;
    ctr = _ctr; // ctr is "dim0" simplex list.
    ctr_moi = ctr -> max_of_index;
    n = ctr -> columns_to_reduce.size();
    print = _print;

    wp = &_wp;

    for (int x = 1; x <= ax; ++x)
    {
      for (int y = 1; y <= ay; ++y)
      {
        for (int type = 0; type < 2; ++type)
        {
          int index = x | (y << 11) | (type << 21);
          double birthday = dcg -> getBirthday(index, 1);
          if (birthday < dcg -> threshold)
          {
            dim1_simplex_list.push_back(BirthdayIndex(birthday, index, 1));
          }
        }
      }
    }

    sort(dim1_simplex_list.rbegin(), dim1_simplex_list.rend(), BirthdayIndexComparator());
  }

  // member method - workhorse
  void joint_pairs_main()
  {
    UnionFind dset(ctr_moi, dcg);
    ctr->columns_to_reduce.clear();
    ctr->dim = 1;
    double min_birth = dcg->threshold;

    for (BirthdayIndex e : dim1_simplex_list)
    {
      int index = e.getIndex();
      int cx = index & 0x07ff;
      int cy = (index >> 11) & 0x03ff;
      int cm = (index >> 21) & 0xff;
      int ce0=0, ce1 =0;

      switch (cm)
      {
      case 0:
        ce0 = ((cy) << 11) | cx;
        ce1 = ((cy) << 11) | (cx + 1);
        break;
      default:
        ce0 = ((cy) << 11) | cx;
      ce1 = ((cy + 1) << 11) | cx;
      break;
      }

      u = dset.find(ce0);
      v = dset.find(ce1);
      if (min_birth >= min(dset.birthtime[u], dset.birthtime[v]))
      {
        min_birth = min(dset.birthtime[u], dset.birthtime[v]);
      }

      if (u != v)
      {
        double birth = max(dset.birthtime[u], dset.birthtime[v]);
        double death = max(dset.time_max[u], dset.time_max[v]);
        if (birth == death)
        {
          dset.link(u, v);
        }
        else
        {
          wp->push_back(WritePairs(0, birth, death));
          dset.link(u, v);
        }
      }
      else // If two values have same "parent", these are potential edges which make a 2-simplex.
      {
        ctr->columns_to_reduce.push_back(e);
      }
    }

    wp->push_back(WritePairs(-1, min_birth, dcg->threshold));
    sort(ctr->columns_to_reduce.begin(), ctr->columns_to_reduce.end(), BirthdayIndexComparator());
  }
};

/*****compute_pairs*****/
template <class Key, class T> class hash_map : public unordered_map<Key, T> {};

class ComputePairs
{
  //member vars
public:
  DenseCubicalGrids* dcg;
  ColumnsToReduce* ctr;
  hash_map<int, int> pivot_column_index;
  int ax, ay;
  int dim;
  vector<WritePairs> *wp;
  bool print;

  // constructor
  ComputePairs(DenseCubicalGrids* _dcg, ColumnsToReduce* _ctr, vector<WritePairs> &_wp, const bool _print)
  {
    dcg = _dcg;
    ctr = _ctr;
    dim = _ctr -> dim;
    wp = &_wp;
    print = _print;

    ax = _dcg -> ax;
    ay = _dcg -> ay;
  }

  // member methods
  //   workhorse
  void compute_pairs_main()
  {
    vector<BirthdayIndex> coface_entries;
    SimplexCoboundaryEnumerator cofaces;
    unordered_map<int, priority_queue<BirthdayIndex, vector<BirthdayIndex>, BirthdayIndexComparator>> recorded_wc;

    pivot_column_index = hash_map<int, int>();
    auto ctl_size = ctr->columns_to_reduce.size();
    pivot_column_index.reserve(ctl_size);
    recorded_wc.reserve(ctl_size);

    for (int i = 0; i < ctl_size; ++i)
    {
      auto column_to_reduce = ctr->columns_to_reduce[i];
      priority_queue<BirthdayIndex, vector<BirthdayIndex>, BirthdayIndexComparator> working_coboundary;
      double birth = column_to_reduce.getBirthday();

      int j = i;
      BirthdayIndex pivot(0, -1, 0);
      bool might_be_apparent_pair = true;
      bool goto_found_persistence_pair = false;

      do {
        auto simplex = ctr->columns_to_reduce[j];// get CTR[i]
        coface_entries.clear();
        cofaces.setSimplexCoboundaryEnumerator(simplex, dcg);// make cofaces data

        while (cofaces.hasNextCoface() && !goto_found_persistence_pair) // repeat there remains a coface
        {
          BirthdayIndex coface = cofaces.getNextCoface();
          coface_entries.push_back(coface);
          if (might_be_apparent_pair && (simplex.getBirthday() == coface.getBirthday())) // if bt is the same, go thru
          {
            if (pivot_column_index.find(coface.getIndex()) == pivot_column_index.end()) // if coface is not in pivot list
            {
              pivot.copyBirthdayIndex(coface);// I have a new pivot
              goto_found_persistence_pair = true;// goto (B)
            }
            else // if pivot list contains this coface,
            {
              might_be_apparent_pair = false;// goto(A)
            }
          }
        }

        if (!goto_found_persistence_pair) // (A) if pivot list contains this coface
        {
          auto findWc = recorded_wc.find(j); // we seek wc list by 'j'
          if (findWc != recorded_wc.end()) // if the pivot is old,
          {
            auto wc = findWc->second;
            while (!wc.empty()) // we push the data of the old pivot's wc
            {
              auto e = wc.top();
              working_coboundary.push(e);
              wc.pop();
            }
          }
          else // if the pivot is new,
          {
            for (auto e : coface_entries) // making wc here
            {
              working_coboundary.push(e);
            }
          }
          pivot = get_pivot(working_coboundary); // getting a pivot from wc

          if (pivot.getIndex() != -1) //When I have a pivot, ...
          {
            auto pair = pivot_column_index.find(pivot.getIndex());
            if (pair != pivot_column_index.end()) // if the pivot already exists, go on the loop
            {
              j = pair->second;
              continue;
            }
            else // if the pivot is new,
            {
              // I record this wc into recorded_wc, and
              recorded_wc.insert(make_pair(i, working_coboundary));
              // I output PP as Writepairs
              double death = pivot.getBirthday();
              outputPP(dim, birth, death);
              pivot_column_index.insert(make_pair(pivot.getIndex(), i));
              break;
            }
          }
          else // if wc is empty, I output a PP as [birth,)
          {
            outputPP(-1, birth, dcg->threshold);
            break;
          }
        }
        else // (B) I have a new pivot and output PP as Writepairs
        {
          double death = pivot.getBirthday();
          outputPP(dim, birth, death);
          pivot_column_index.insert(make_pair(pivot.getIndex(), i));
          break;
        }

      } while (true);
    }
  }

  void outputPP(int _dim, double _birth, double _death)
  {
    if (_birth != _death)
    {
      if (_death != dcg-> threshold)
      {
        wp->push_back(WritePairs(_dim, _birth, _death));
      }
      else
      {
        wp->push_back(WritePairs(-1, _birth, dcg -> threshold));
      }
    }
  }

  BirthdayIndex pop_pivot(priority_queue<BirthdayIndex, vector<BirthdayIndex>, BirthdayIndexComparator>& column)
  {
    if (column.empty())
    {
      return BirthdayIndex(0, -1, 0);
    }
    else
    {
      auto pivot = column.top();
      column.pop();

      while (!column.empty() && column.top().index == pivot.getIndex())
      {
        column.pop();
        if (column.empty())
          return BirthdayIndex(0, -1, 0);
        else
        {
          pivot = column.top();
          column.pop();
        }
      }
      return pivot;
    }
  }

  BirthdayIndex get_pivot(priority_queue<BirthdayIndex, vector<BirthdayIndex>, BirthdayIndexComparator>& column)
  {
    BirthdayIndex result = pop_pivot(column);
    if (result.getIndex() != -1)
    {
      column.push(result);
    }
    return result;
  }

  void assemble_columns_to_reduce()
  {
    ++dim;
    ctr->dim = dim;
    const int typenum = 2;
    if (dim == 1)
    {
      ctr->columns_to_reduce.clear();
      for (int y = 1; y <= ay; ++y)
      {
        for (int x = 1; x <= ax; ++x)
        {
          for (int m = 0; m < typenum; ++m)
          {
            double index = x | (y << 11) | (m << 21);
            if (pivot_column_index.find(index) == pivot_column_index.end())
            {
              double birthday = dcg -> getBirthday(index, 1);
              if (birthday != dcg -> threshold)
              {
                ctr -> columns_to_reduce.push_back(BirthdayIndex(birthday, index, 1));
              }
            }
          }
        }
      }
    }
    sort(ctr -> columns_to_reduce.begin(), ctr -> columns_to_reduce.end(), BirthdayIndexComparator());
  }
};

// method = 0 --> link find algo (default)
// method = 1 --> compute pairs algo
// [[Rcpp::export]]
Rcpp::NumericMatrix cubical_2dim(const Rcpp::NumericMatrix& image, double threshold, int method)
{
  bool print = false;

  vector<WritePairs> writepairs; // dim birth death
  writepairs.clear();

  DenseCubicalGrids* dcg = new DenseCubicalGrids(image, threshold);
  ColumnsToReduce* ctr = new ColumnsToReduce(dcg);

  switch(method)
  {
    case 0:
    {
      JointPairs* jp = new JointPairs(dcg, ctr, writepairs, print);
      jp->joint_pairs_main(); // dim0

      ComputePairs* cp = new ComputePairs(dcg, ctr, writepairs, print);
      cp->compute_pairs_main(); // dim1

      break;
    }

    case 1:
    {
      ComputePairs* cp = new ComputePairs(dcg, ctr, writepairs, print);
      cp->compute_pairs_main(); // dim0
      cp->assemble_columns_to_reduce();

      cp->compute_pairs_main(); // dim1

      break;
    }
  }

  Rcpp::NumericMatrix ans(writepairs.size(), 3);
  for (int i = 0; i < ans.nrow(); i++)
  {
    ans(i, 0) = writepairs[i].getDimension();
    ans(i, 1) = writepairs[i].getBirth();
    ans(i, 2) = writepairs[i].getDeath();
  }
  return ans;
}