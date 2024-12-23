// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*
  Copyright 2024 Equinor ASA.

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.

  Consult the COPYING file in the top-level source directory of this
  module for the precise wording of the license and the list of
  copyright holders.
*/

#include <config.h>

#include <dune/common/version.hh>

#define BOOST_TEST_MODULE GraphRepresentationOfGridParallel
#define BOOST_TEST_NO_MAIN
#include <boost/test/unit_test.hpp>
#include <opm/grid/CpGrid.hpp>

#include <opm/grid/GraphOfGrid.hpp>
#include <opm/grid/GraphOfGridWrappers.hpp>

#if HAVE_MPI
// After partitioning, importList and exportList are not complete,
// other cells from wells need to be added.
BOOST_AUTO_TEST_CASE(ImportExportListExpansion)
{
    Dune::CpGrid grid;
    std::array<int,3> dims{3,3,2};
    std::array<double,3> size{1.,1.,1.};
    grid.createCartesian(dims,size);
    const auto& cc = grid.comm();
    if (cc.size()==1)
        return;

    Opm::GraphOfGrid gog(grid);
    // grid is nonempty only on the rank 0
    if (cc.rank()==0)
    {
        gog.addWell(std::set<int>{0,1,2});
        gog.addWell(std::set<int>{3,4,5});
        gog.addWell(std::set<int>{6,7,8});
        gog.addWell(std::set<int>{9,13,17});
        BOOST_REQUIRE(gog.size()==10);
    }

    // rank-specific export and import lists
    std::vector<std::tuple<int,int,char>> exportList, exportSolution;
    std::vector<std::tuple<int,int,char,int>> importList, importSolution;
    // this test works on any number or ranks although from rank 4 (including) all are empty
    // if ranks<4, the highest rank gobbles leftovers
    int maxrank = cc.size()-1;
    std::vector<int> ranks{0,std::min(maxrank,1),std::min(maxrank,2),std::min(maxrank,3)};

    // cells[rank] holds solution, each rank 0..3 has 1 well
    std::vector<std::vector<int>> cells(4);
    cells[0] = std::vector<int>{0,1,2,10,11};
    cells[1] = std::vector<int>{3,4,5,12};
    cells[2] = std::vector<int>{6,7,8,15,16};
    cells[3] = std::vector<int>{9,13,14,17};
    using AttributeSet = Dune::cpgrid::CpGridData::AttributeSet;
    char owner = AttributeSet::owner;

    for (int i=0; i<4; ++i)
    {
        for (const auto& c : cells[i])
        {
            if (cc.rank()==ranks[i])
            {
                importSolution.push_back(std::make_tuple(c,ranks[i],owner,-1));
            }
            if (cc.rank()==0)
            {
                exportSolution.push_back(std::make_tuple(c,ranks[i],owner));
            }
        }
    }
    std::sort(importSolution.begin(),importSolution.end());
    std::sort(exportSolution.begin(),exportSolution.end());

    if (cc.rank()==ranks[0])
    {
        // Zoltan does not include cells that remain on the rank into import and export list
        // but they are added manually to both lists (cell on root is in its import AND export)
        // before extendExportAndImportLists is called
        for (const auto& gID : cells[ranks[0]])
        {
            importList.push_back(std::make_tuple(gID,ranks[0],owner,-1));
            exportList.push_back(std::make_tuple(gID,ranks[0],owner));
        }
        exportList.push_back(std::make_tuple(3,ranks[1],owner));
        exportList.push_back(std::make_tuple(12,ranks[1],owner));
        exportList.push_back(std::make_tuple(6,ranks[2],owner));
        exportList.push_back(std::make_tuple(15,ranks[2],owner));
        exportList.push_back(std::make_tuple(16,ranks[2],owner));
        exportList.push_back(std::make_tuple(9,ranks[3],owner));
        exportList.push_back(std::make_tuple(14,ranks[3],owner));
        std::sort(exportList.begin(),exportList.end());
    }
    else if (cc.rank()==ranks[1])
    {
        // non-root ranks have empty export list
        // import list is not sorted
        importList.push_back(std::make_tuple(12,ranks[1],owner,-1));
        importList.push_back(std::make_tuple(3,ranks[1],owner,-1));
    }
    // no else branch, ranks[2]==1 when cc.size==2
    if (cc.rank()==ranks[2])
    {
        importList.push_back(std::make_tuple(15,ranks[2],owner,-1));
        importList.push_back(std::make_tuple(6,ranks[2],owner,-1));
        importList.push_back(std::make_tuple(16,ranks[2],owner,-1));
    }
    if (cc.rank()==ranks[3])
    {
        importList.push_back(std::make_tuple(9,ranks[3],owner,-1));
        importList.push_back(std::make_tuple(14,ranks[3],owner,-1));
    }

    extendExportAndImportLists(gog,cc,ranks[0],exportList,importList);

    BOOST_CHECK_MESSAGE(importList==importSolution,"On rank "+std::to_string(cc.rank()));
    BOOST_CHECK_MESSAGE(exportList==exportSolution,"On rank "+std::to_string(cc.rank()));
}
#endif // HAVE_MPI

bool
init_unit_test_func()
{
    return true;
}

int main(int argc, char** argv)
{
    Dune::MPIHelper::instance(argc, argv);
    boost::unit_test::unit_test_main(&init_unit_test_func,
                                     argc, argv);
}
