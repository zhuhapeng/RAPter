#include "globfit2/util/parse.h"
#include "globfit2/globOpt_types.h" // _2d::, _3d::

// ________________correspondance___________________

#include "globfit2/io/io.h"         // readPrimitives()
#include "globfit2/util/diskUtil.hpp" // saveBackup()

namespace GF2 {
namespace correspondence
{
    template <typename Scalar, class _PrimitiveT, class _PointContainerT>
    inline Scalar estimateDistance( _PrimitiveT const& prim, _PrimitiveT const& gt_prim, _PointContainerT const& points, int pnt_gid, int gt_pnt_gid )
    {
        std::cout << "pos: " << prim.template pos().transpose() << ", gtpos: " << gt_prim.template pos().transpose() << ", norm: " << (prim.template pos() - gt_prim.template pos()).norm() << std::endl;
        return (prim.template pos() - gt_prim.template pos()).norm();
    }

    template < typename _PrimitiveT
             , class    _InnerPrimitiveContainerT
             , class    _PrimitiveContainerT
             , class    _PointPrimitiveT
             , class    _PointContainerT
             >
    int correspCli( int argc, char**argv )
    {
        // usual <gid, vector<primitive> > map
        typedef std::map<int, _InnerPrimitiveContainerT> PrimitiveMapT;
        // points belong to two primitives
        // (one from primtivesA and one from primitivesB),
        // so we need a second key for that)
        enum { PNT_GID_B    = _PointPrimitiveT::GID
             , PNT_GID_A    = _PointPrimitiveT::USER_ID1
             };

        // first: GID (map key), second: linearId in vector map[gid].
        typedef std::pair< int   , int    > GidLid;   //!< Uniquely identifies an entry in PrimitiveMapT.
        // first: primitiveA, second: primitiveB
        typedef std::map < GidLid, GidLid > CorrespT; //!< Output type, contains primitive-gt correspondances. Key: <PrimitiveGid,PrimitiveLid> => Value: <GTGid,GTLid>

        int err = EXIT_SUCCESS;

        // print usage
        if (    GF2::console::find_switch(argc,argv,"-h")
             || GF2::console::find_switch(argc,argv,"--help")
             || (argc != 6) )
        {
            std::cout << "Usage: "
                      << argv[0] << "\n"
                      << " primsA.csv \n"
                      << " points_primitivesA.csv\n"
                      << " primsB.csv \n"
                      << " points_primitivesB.csv\n"
                      << " cloud.ply\n"
                      ;
            return err;
        } //...print usage

        // parse input
        std::string prims_pathA,
                    prims_pathB,
                    cloud_path = "./cloud.ply",
                    assoc_pathB,
                    assoc_pathA;
        {
            //if ( GF2::console::parse_argument(argc,argv,"--gt",gt_path) < 0 )
            prims_pathA = std::string( argv[1] );
            if ( !boost::filesystem::exists(prims_pathA) )
            {
                std::cerr << "[" << __func__ << "]: " << "need prims_pathA" << prims_pathA << " to exist!" << std::endl;
                return EXIT_FAILURE;
            }

            // if ( GF2::console::parse_argument(argc,argv,"--gta",gt_assoc_path) < 0 )
            assoc_pathA = std::string( argv[2] );
            if ( !boost::filesystem::exists(assoc_pathA) )
            {
                std::cerr << "[" << __func__ << "]: " << "need assoc_pathA" << assoc_pathA << " to exist!" << std::endl;
                return EXIT_FAILURE;
            }

            //if ( GF2::console::parse_argument(argc,argv,"--p",prims_path) < 0 )
            prims_pathB = std::string( argv[3] );
            if ( !boost::filesystem::exists(prims_pathB) )
            {
                std::cerr << "[" << __func__ << "]: " << "need prims_pathB" << prims_pathB << "to exist!" << std::endl;
                return EXIT_FAILURE;
            }

            //if ( GF2::console::parse_argument(argc,argv,"--pa", assoc_path) < 0 )
            assoc_pathB = std::string( argv[4] );
            if ( !boost::filesystem::exists(assoc_pathB) )
            {
                std::cerr << "[" << __func__ << "]: " << "need assoc_pathB " << assoc_pathB << " to exist!" << std::endl;
                return EXIT_FAILURE;
            }

            //if ( (GF2::console::parse_argument(argc,argv,"--cloud",cloud_path) < 0) && !boost::filesystem::exists(cloud_path) )
            cloud_path = std::string( argv[5] );
            if ( !boost::filesystem::exists(cloud_path) )
            {
                std::cerr << "[" << __func__ << "]: " << "need cloud_path " << cloud_path << "to exist!" << std::endl;
                return EXIT_FAILURE;
            }
        } //...parse input

        _PointContainerT     points;
        PrimitiveMapT        prims_mapA, prims_mapB;
        // read input
        {
            // Read points
            if ( EXIT_SUCCESS == err )
            {
                err = io::readPoints<_PointPrimitiveT>( points, cloud_path );
                if ( err != EXIT_SUCCESS )  std::cerr << "[" << __func__ << "]: " << "readPoints returned error " << err << std::endl;
            } //...read points

            // tmp var to read to, first: point id, second: primitive GID
            std::vector<std::pair<int,int> > points_primitives;

            // read A associations
            {
                io::readAssociations( points_primitives, assoc_pathA, NULL );
                for ( size_t i = 0; i != points.size(); ++i )
                {
                    // store association in point
                    points[i].setTag( PNT_GID_A, points_primitives[i].first );
                }
            }

            // read B associations
            {
                io::readAssociations( points_primitives, assoc_pathB, NULL );
                for ( size_t i = 0; i != points.size(); ++i )
                {
                    // store association in point
                    points[i].setTag( PNT_GID_B, points_primitives[i].first );
                }
            }

            // read primitives
            _PrimitiveContainerT primitivesA, primitivesB; // unused, so local scope
            {
                // A
                std::cout << "[" << __func__ << "]: " << "reading primitivesA from " << prims_pathA << "...";
                io::readPrimitives<_PrimitiveT, _InnerPrimitiveContainerT>( primitivesA, prims_pathA, &prims_mapA );
                std::cout << "reading primitivesA ok (#: " << prims_mapA.size() << ")\n";

                // B
                std::cout << "[" << __func__ << "]: " << "reading primitivesB from " << prims_pathB << "...";
                io::readPrimitives<_PrimitiveT, _InnerPrimitiveContainerT>( primitivesB, prims_pathB, &prims_mapB );
                std::cout << "reading primitivesB ok (#: " << prims_mapB.size() << ")\n";
            } //...read primitives
        }

        // ________________________WORK____________________________

        // output correspondences. first: primA(gid,lid), second: primB(gid,lid)
        CorrespT corresps;
        // iterator over patches
        typedef typename PrimitiveMapT::const_iterator             outer_const_iterator;
        // iterator over primitives in patch (have same GID)
        typedef typename _InnerPrimitiveContainerT::const_iterator inner_const_iterator;

        // cost float type
        typedef typename _PointPrimitiveT::Scalar Scalar;
        typedef std::pair< GidLid , GidLid >      CostKey; // first: primitiveA, second: primitiveB
        typedef std::map < CostKey, Scalar >      CostMap; // < <primAGid,primALid>,<primBGid,primBLid> > => cost           // watch the order! <primA, primB>

        // store costs in a retrievable map ( we could use a vector right away, but we might need it later )
        CostMap costs;
        {
            // calc distances
            int gidA, gidB, lidA, lidB;
            // for patches in A
            for ( outer_const_iterator outer_it0 = prims_mapA.begin(); outer_it0 != prims_mapA.end(); ++outer_it0 )
            {
                // cache patch id gidA
                gidA = (*outer_it0).first;
                // start linear primitive id from 0 in this patch
                lidA = 0;
                // for primtives in patch of A
                for ( inner_const_iterator inner_it0 = (*outer_it0).second.begin(); inner_it0 != (*outer_it0).second.end(); ++inner_it0, ++lidA )
                {
                    // for patches in B
                    for ( outer_const_iterator outer_it1 = prims_mapB.begin(); outer_it1 != prims_mapB.end(); ++outer_it1 )
                    {
                        // cache patch id gidB
                        gidB = (*outer_it1).first;
                        // start linear primitive id from 0 in this patch
                        lidB = 0;
                        // for primitives in patch of B
                        for ( inner_const_iterator inner_it1 = (*outer_it1).second.begin(); inner_it1 != (*outer_it1).second.end(); ++inner_it1, ++lidB )
                        {
                            // log
                            std::cout << "checking " << gidA << "." << lidA << " vs " << gidB << "." << lidB;

                            // calculate cost and insert into map
                            costs[ CostKey(GidLid(gidA,lidA),GidLid(gidB,lidB)) ] = estimateDistance<float>( *inner_it0, *inner_it1, points, PNT_GID_A, PNT_GID_B );

                            // log
                            std::cout << ": " << costs[ CostKey(GidLid(gidA,lidA),GidLid(gidB,lidB)) ] << std::endl;
                        } //...for prims in patchB
                    } //...for patchesB
                } //...for prims in patchA
            } //...for patchesA
        } //...calculate costs

        // we now have the costs in a map,
        // but we need them in a sortable list,
        // where the cost is the key

        // create sorted cost keyed list
        typedef std::pair<Scalar,CostKey> CostEntry; // first: cost, second: < <gidA,lidA>,<gidB,lidB> >
        std::vector< CostEntry > cost_list;          // store entries by cost
        {
            for ( typename CostMap::const_iterator it = costs.begin(); it != costs.end(); ++it )
            {
                cost_list.push_back( CostEntry(it->second,it->first) );
            }

            // sort by cost
            std::sort( cost_list.begin(), cost_list.end() );
        } //...sorted cost list

        // calculate correspondences
        {
            // select non-taken pairs
            std::set<GidLid> taken_primsA, taken_primsB;
            for ( int i = 0; i < cost_list.size(); ++i )
            {
                CostKey const& costKey   = cost_list[i].second; // second: < <gidA,lidA>,<gidB,lidB> >
                GidLid  const& gidLidA   = costKey.first;       // <gidA,lidA>
                GidLid  const& gidLidB   = costKey.second;      // <gidB,lidB>

                // log
                //std::cout << "cost_list[" << i << "]: " << cost_list[i].first << " for "
                //          << gidLidA.first << "." << gidLidA.second << " - "
                //          << gidLidB.first << "." << gidLidB.second << std::endl;

                // use, if *both* free to pair up
                if (    (taken_primsA.find(gidLidA) == taken_primsA.end())
                     && (taken_primsB.find(gidLidB) == taken_primsB.end()) )
                {
                    // log
                    std::cout << "chose " << cost_list[i].first << " for "
                              << gidLidA.first << "." << gidLidA.second << " - "
                              << gidLidB.first << "." << gidLidB.second << std::endl;

                    // make them "unfree"
                    taken_primsA.insert( gidLidA );
                    taken_primsB.insert( gidLidB );

                    // debug
                    if ( corresps.find(gidLidA) != corresps.end() )
                        std::cerr << "[" << __func__ << "]: " << "duplicate choice...should not happen!" << std::endl;

                    // save correspondence
                    corresps[ gidLidA ] = gidLidB;
                } //...if both free
            } //...for each cost entry
        } //...create corresps

        // print
        {
            // create output path
            std::string corresp_path = "./corresp.csv";
            // backup previous copy
            GF2::util::saveBackup( corresp_path );

            // open file
            std::ofstream corresp_f( corresp_path );
            // check if opened
            if ( !corresp_f.is_open() )
            {
                std::cerr << "[" << __func__ << "]: " << "could not open " << corresp_path << std::endl;
                return EXIT_FAILURE;
            }

            // debug
            PrimitiveMapT subs;

            // log
            corresp_f << "# corresp between\n# " << prims_pathA << "," << prims_pathB << std::endl;
            // for each correspondence
            for ( CorrespT::const_iterator it = corresps.begin(); it != corresps.end(); ++it )
            {
                // cache ids
                GidLid const& gidLidA = (*it).first;  // first: <gidA,lidA>
                GidLid const& gidLidB = (*it).second; // second: <gidB,lidB>

                // log
                std::cout << "prims[" << gidLidA.first << "][" << gidLidA.second << "]: " << prims_mapA[gidLidA.first][gidLidA.second].toString()
                          << " <--> "
                          << "gt[" << gidLidB.first << "][" << gidLidB.second << "]: " << prims_mapB[gidLidB.first][gidLidB.second].toString()
                          << " with cost " << costs[ CostKey(gidLidA,gidLidB) ]
                          << std::endl;

                // write to file
                corresp_f << gidLidA.first  << ","
                          << gidLidA.second << ","
                          << gidLidB.first  << ","
                          << gidLidB.second << "\n";

                // debug
                containers::add( subs, gidLidA.first, prims_mapB[gidLidB.first][gidLidB.second] );
            }

            // close file
            corresp_f.close();

            // debug
            GF2::io::savePrimitives<_PrimitiveT,typename _InnerPrimitiveContainerT::const_iterator>( subs, "subs.csv" );
        } //...print

        return EXIT_SUCCESS;
    } //...correspCli()

} //...namespace correspondance
} //...namespace GF2

int main( int argc, char** argv )
{
    if ( GF2::console::find_switch(argc,argv,"--corresp3D") )
    {
        std::cerr << "corresp3D unimplemented" << std::endl;
    }
    else
    {
        return GF2::correspondence::correspCli< GF2::_2d::PrimitiveT
                                              , GF2::_2d::InnerPrimitiveContainerT
                                              , GF2::_2d::PrimitiveContainerT
                                              , GF2::PointPrimitiveT
                                              , GF2::PointContainerT
                                              >( argc, argv );
    }

    return EXIT_FAILURE;
} //...corresp()
