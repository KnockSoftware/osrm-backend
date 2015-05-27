/*

Copyright (c) 2015, Project OSRM contributors
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef ROUND_TRIP_HPP
#define ROUND_TRIP_HPP

#include "plugin_base.hpp"

#include "../algorithms/object_encoder.hpp"
#include "../data_structures/query_edge.hpp"
#include "../data_structures/search_engine.hpp"
#include "../descriptors/descriptor_base.hpp"
#include "../descriptors/json_descriptor.hpp"
#include "../util/json_renderer.hpp"
#include "../util/make_unique.hpp"
#include "../util/string_util.hpp"
#include "../util/timing_util.hpp"
#include "../util/simple_logger.hpp"

#include <osrm/json_container.hpp>

#include <cstdlib>

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <limits> 

template <class DataFacadeT> class RoundTripPlugin final : public BasePlugin
{
  private:
    std::string descriptor_string;
    DataFacadeT *facade;
    std::unique_ptr<SearchEngine<DataFacadeT>> search_engine_ptr;

    void FarthestInsertion(const RouteParameters & route_parameters,
                           const PhantomNodeArray & phantom_node_vector,
                           std::vector<EdgeWeight> & dist_table, 
                           InternalRouteResult & min_route,
                           std::vector<int> & min_loc_permutation) {
        //////////////////////////////////////////////////////////////////////////////////////////////////
        // START FARTHEST INSERTION HERE
        // 1. start at a random round trip of 2 locations
        // 2. find the location that is the farthest away from the visited locations
        // 3. add the found location to the current round trip such that round trip is the shortest
        // 4. repeat 2-3 until all locations are visited
        // 5. DONE!
        //////////////////////////////////////////////////////////////////////////////////////////////////
    }

    void NearestNeighbour(const RouteParameters & route_parameters,
                          const PhantomNodeArray & phantom_node_vector,
                          std::vector<EdgeWeight> & dist_table, 
                          InternalRouteResult & min_route,
                          std::vector<int> & min_loc_permutation) {
        //////////////////////////////////////////////////////////////////////////////////////////////////
        // START GREEDY NEAREST NEIGHBOUR HERE
        // 1. grab a random location and mark as starting point
        // 2. find the nearest unvisited neighbour, set it as the current location and mark as visited
        // 3. repeat 2 until there is no unvisited location
        // 4. return route back to starting point
        // 5. compute route
        // 6. repeat 1-5 with different starting points and choose iteration with shortest trip
        // 7. DONE!
        //////////////////////////////////////////////////////////////////////////////////////////////////

        const auto number_of_locations = phantom_node_vector.size();
        min_route.shortest_path_length = std::numeric_limits<int>::max();

        // is_lonely_island[i] indicates whether node i is a node that cannot be reached from other nodes
        //  1 means that node i is a lonely island
        //  0 means that it is not known for node i
        // -1 means that node i is not a lonely island but a reachable, connected node
        std::vector<int> is_lonely_island(number_of_locations, 0);
        int count_unreachables;

        // ALWAYS START AT ANOTHER STARTING POINT
        for(int start_node = 0; start_node < number_of_locations; ++start_node)
        {
        
            if (is_lonely_island[start_node] >= 0)
            {
                // if node is a lonely island it is an unsuitable node to start from and shall be skipped
                if (is_lonely_island[start_node])
                    continue;
                count_unreachables = 0;
                auto start_dist_begin = dist_table.begin() + (start_node * number_of_locations);
                auto start_dist_end = dist_table.begin() + ((start_node + 1) * number_of_locations);
                for (auto it2 = start_dist_begin; it2 != start_dist_end; ++it2) {
                    if (*it2 == 0 || *it2 == std::numeric_limits<int>::max()) {
                        ++count_unreachables;
                    }
                }
                if (count_unreachables >= number_of_locations) {
                    is_lonely_island[start_node] = 1;
                    continue;
                }
            }

            int curr_node = start_node;   
            is_lonely_island[curr_node] = -1;
            InternalRouteResult raw_route;
            //TODO: Should we always use the same vector or does it not matter at all because of loop scope?
            std::vector<int> loc_permutation(number_of_locations, -1);
            loc_permutation[start_node] = 0;
            // visited[i] indicates whether node i was already visited by the salesman
            std::vector<bool> visited(number_of_locations, false);
            visited[start_node] = true;

            PhantomNodes viapoint;
            // 3. REPEAT FOR EVERY UNVISITED NODE
            for(int via_point = 1; via_point < number_of_locations; ++via_point)
            {
                int min_dist = std::numeric_limits<int>::max();
                int min_id = -1;

                // 2. FIND NEAREST NEIGHBOUR
                auto row_begin_iterator = dist_table.begin() + (curr_node * number_of_locations);
                auto row_end_iterator = dist_table.begin() + ((curr_node + 1) * number_of_locations);
                for (auto it = row_begin_iterator; it != row_end_iterator; ++it) {
                    auto index = std::distance(row_begin_iterator, it); 
                    if (is_lonely_island[index] < 1 && !visited[index] && *it < min_dist)
                    {
                        min_dist = *it;
                        min_id = index;
                    }
                }
                // in case there was no unvisited and reachable node found, it means that all remaining (unvisited) nodes must be lonely islands
                if (min_id == -1)
                {
                    for(int loc = 0; loc < visited.size(); ++loc) {
                        if (!visited[loc]) {
                            is_lonely_island[loc] = 1;
                        }
                    }
                    break;
                }
                // set the nearest unvisited location as the next via_point
                else
                {
                    is_lonely_island[min_id] = -1;
                    loc_permutation[min_id] = via_point;
                    visited[min_id] = true;
                    viapoint = PhantomNodes{phantom_node_vector[curr_node][0], phantom_node_vector[min_id][0]};
                    raw_route.segment_end_coordinates.emplace_back(viapoint);
                    curr_node = min_id;
                }
            }

            // 4. ROUTE BACK TO STARTING POINT
            viapoint = PhantomNodes{raw_route.segment_end_coordinates.back().target_phantom, phantom_node_vector[start_node][0]};
            raw_route.segment_end_coordinates.emplace_back(viapoint);

            // 5. COMPUTE ROUTE
            search_engine_ptr->shortest_path(raw_route.segment_end_coordinates, route_parameters.uturns, raw_route);
            
            // check round trip with this starting point is shorter than the shortest round trip found till now
            if (raw_route.shortest_path_length < min_route.shortest_path_length) {
                min_route = raw_route;
                min_loc_permutation = loc_permutation;
            }
        }
    }

  public:
    explicit RoundTripPlugin(DataFacadeT *facade)
        : descriptor_string("trip"), facade(facade)
    {
        search_engine_ptr = osrm::make_unique<SearchEngine<DataFacadeT>>(facade);
    }

    const std::string GetDescriptor() const override final { return descriptor_string; }

    int HandleRequest(const RouteParameters &route_parameters,
                      osrm::json::Object &json_result) override final
    {
        // check if all inputs are coordinates
        if (!check_all_coordinates(route_parameters.coordinates))
        {
            return 400;
        }
        const bool checksum_OK = (route_parameters.check_sum == facade->GetCheckSum());

        // find phantom nodes for all input coords
        PhantomNodeArray phantom_node_vector(route_parameters.coordinates.size());
        for (const auto i : osrm::irange<std::size_t>(0, route_parameters.coordinates.size()))
        {
            // if client hints are helpful, encode hints
            if (checksum_OK && i < route_parameters.hints.size() &&
                !route_parameters.hints[i].empty())
            {
                PhantomNode current_phantom_node;
                ObjectEncoder::DecodeFromBase64(route_parameters.hints[i], current_phantom_node);
                if (current_phantom_node.is_valid(facade->GetNumberOfNodes()))
                {
                    phantom_node_vector[i].emplace_back(std::move(current_phantom_node));
                    continue;
                }
            }
            facade->IncrementalFindPhantomNodeForCoordinate(route_parameters.coordinates[i],
                                                            phantom_node_vector[i], 1);
            if (phantom_node_vector[i].size() > 1)
            {
                phantom_node_vector[i].erase(phantom_node_vector[i].begin());
            }
            BOOST_ASSERT(phantom_node_vector[i].front().is_valid(facade->GetNumberOfNodes()));
        }

        // compute the distance table of all phantom nodes
        const std::shared_ptr<std::vector<EdgeWeight>> result_table =
            search_engine_ptr->distance_table(phantom_node_vector);

        if (!result_table)
        {
            return 400;
        }

        // compute TSP round trip
        InternalRouteResult min_route;
        std::vector<int> min_loc_permutation;
        TIMER_START(tsp_nn);
        NearestNeighbour(route_parameters, phantom_node_vector, *result_table, min_route, min_loc_permutation);
        TIMER_STOP(tsp_nn);

        SimpleLogger().Write() << "Distance " << min_route.shortest_path_length;
        SimpleLogger().Write() << "Time " << TIMER_MSEC(tsp_nn);

        // return result to json
        std::unique_ptr<BaseDescriptor<DataFacadeT>> descriptor;
        descriptor = osrm::make_unique<JSONDescriptor<DataFacadeT>>(facade);
        
        descriptor->SetConfig(route_parameters);
        descriptor->Run(min_route, json_result);

        osrm::json::Array json_loc_permutation;
        json_loc_permutation.values.insert(json_loc_permutation.values.end(), min_loc_permutation.begin(), min_loc_permutation.end());
        json_result.values["nn_loc_permutation"] = json_loc_permutation;
        json_result.values["nn_distance"] = min_route.shortest_path_length;
        json_result.values["nn_runtime"] = TIMER_MSEC(tsp_nn);

        return 200;
    }

};

#endif // ROUND_TRIP_HPP