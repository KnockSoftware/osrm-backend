/*

Copyright (c) 2014, Project OSRM, Kirill Zhdanovich
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


#ifndef PBF_DESCRIPTOR_H
#define PBF_DESCRIPTOR_H

#include "Descriptor.h"
#include "response.pb.h"

template <class DataFacadeT> class PBFDescriptor : public Descriptor<DataFacadeT>
{
  private:

    inline void AddInstructionToRoute(protobufResponse::Route &route,
                                      const std::string &id,
                                      const std::string &streetName,
                                      const int &length,
                                      const unsigned &position,
                                      const int &time,
                                      const std::string &lengthStr,
                                      const std::string &earthDirection,
                                      const int &azimuth)
    {
        protobufResponse::RouteInstructions routeInstructions;
        routeInstructions.set_instruction_id(id);
        routeInstructions.set_street_name(streetName);
        routeInstructions.set_length(length);
        routeInstructions.set_position(position);
        routeInstructions.set_time(time);
        routeInstructions.set_length_str(lengthStr);
        routeInstructions.set_earth_direction(earthDirection);
        routeInstructions.set_azimuth(azimuth);
        route.add_route_instructions()->CopyFrom(routeInstructions);

    }

    inline void BuildTextualDescription(const int route_length,
                                        DescriptionFactory &description_factory,
                                        std::vector<typename Descriptor<DataFacadeT>::Segment> &route_segments_list,
                                        protobufResponse::Route &route
                                        )
    {
        std::vector<typename Descriptor<DataFacadeT>::Instruction> instructions;
        Descriptor<DataFacadeT>::BuildTextualDescription(description_factory,
                                                         instructions,
                                                         route_length,
                                                         route_segments_list);

        for (const typename Descriptor<DataFacadeT>::Instruction &i : instructions)
        {
            AddInstructionToRoute(route, i.instructionId, i.streetName, i.length, 
                                  i.position, i.time, i.lengthStr, i.earthDirection,
                                  i.azimuth);
        }
    }

  public:
    PBFDescriptor(DataFacadeT *facade) : Descriptor<DataFacadeT>(facade)
    {}

    void Run(const RawRouteData &raw_route, http::Reply &reply)
    {
        protobufResponse::Response response;
        std::string output;

        if (INVALID_EDGE_WEIGHT == raw_route.shortest_path_length)
        {
            // We do not need to do much, if there is no route ;-)
            response.set_status(207);
            response.set_status_message("Cannot find route between points");
            response.SerializeToString(&output);
            reply.content.insert(reply.content.end(), output.begin(), output.end());
            return;
        }

        std::string road_name = Descriptor<DataFacadeT>::facade->GetEscapedNameForNameID(
            raw_route.segment_end_coordinates.front().source_phantom.name_id);

        BOOST_ASSERT(raw_route.unpacked_path_segments.size() ==
                     raw_route.segment_end_coordinates.size());

        Descriptor<DataFacadeT>::description_factory.SetStartSegment(
            raw_route.segment_end_coordinates.front().source_phantom,
            raw_route.source_traversed_in_reverse.front());
        response.set_status(0);
        response.set_status_message("Found route between points");

        // for each unpacked segment add the leg to the description
        for (const auto i : osrm::irange<std::size_t>(0, raw_route.unpacked_path_segments.size()))
        {
#ifndef NDEBUG
            const int added_segments =
#endif
            Descriptor<DataFacadeT>::DescribeLeg(raw_route.unpacked_path_segments[i],
                        raw_route.segment_end_coordinates[i],
                        raw_route.target_traversed_in_reverse[i],
                        raw_route.is_via_leg(i));
            BOOST_ASSERT(0 < added_segments);
        }

        protobufResponse::Route mainRoute;
        Descriptor<DataFacadeT>::description_factory.Run(Descriptor<DataFacadeT>::facade, Descriptor<DataFacadeT>::config.zoom_level);
        if (Descriptor<DataFacadeT>::config.geometry)
        {
            std::string route_geometry;
            Descriptor<DataFacadeT>::description_factory.AppendEncodedPolylineStringEncoded(route_geometry);
            mainRoute.set_route_geometry(route_geometry);
        }
        if (Descriptor<DataFacadeT>::config.instructions)
        {
            BuildTextualDescription(raw_route.shortest_path_length,
                                    Descriptor<DataFacadeT>::description_factory,
                                    Descriptor<DataFacadeT>::shortest_path_segments,
                                    mainRoute);
        }

        Descriptor<DataFacadeT>::description_factory.BuildRouteSummary(Descriptor<DataFacadeT>::description_factory.entireLength,
                                              raw_route.shortest_path_length);

        protobufResponse::RouteSummary routeSummary;

        routeSummary.set_total_distance(Descriptor<DataFacadeT>::description_factory.summary.distance);
        routeSummary.set_total_time(Descriptor<DataFacadeT>::description_factory.summary.duration);
        routeSummary.set_start_point(Descriptor<DataFacadeT>::facade->GetEscapedNameForNameID(Descriptor<DataFacadeT>::description_factory.summary.source_name_id));
        routeSummary.set_end_point(Descriptor<DataFacadeT>::facade->GetEscapedNameForNameID(Descriptor<DataFacadeT>::description_factory.summary.target_name_id));
        mainRoute.mutable_route_summary()->CopyFrom(routeSummary);

        BOOST_ASSERT(!raw_route.segment_end_coordinates.empty());

        protobufResponse::Point point;
        point.set_lat(raw_route.segment_end_coordinates.front().source_phantom.location.lat /
                                             COORDINATE_PRECISION);
        point.set_lon(raw_route.segment_end_coordinates.front().source_phantom.location.lon /
                                             COORDINATE_PRECISION);
        mainRoute.add_via_points()->CopyFrom(point);


        for (const PhantomNodes &nodes : raw_route.segment_end_coordinates)
        {
            point.set_lat(nodes.target_phantom.location.lat /
                                             COORDINATE_PRECISION);
            point.set_lon(nodes.target_phantom.location.lon /
                                             COORDINATE_PRECISION);
            mainRoute.add_via_points()->CopyFrom(point);
        }

        std::vector<unsigned> const &shortest_leg_end_indices = Descriptor<DataFacadeT>::description_factory.GetViaIndices();
        for (unsigned v : shortest_leg_end_indices)
        {
            mainRoute.add_via_indices(v);
        }

        RouteNames route_names =
            Descriptor<DataFacadeT>::GenerateRouteNames(Descriptor<DataFacadeT>::shortest_path_segments, 
                Descriptor<DataFacadeT>::alternative_path_segments, Descriptor<DataFacadeT>::facade);

        if (INVALID_EDGE_WEIGHT != raw_route.alternative_path_length)
        {
            protobufResponse::Route alternativeRoute;
            BOOST_ASSERT(!raw_route.alt_source_traversed_in_reverse.empty());
            Descriptor<DataFacadeT>::alternate_description_factory.SetStartSegment(
                raw_route.segment_end_coordinates.front().source_phantom,
                raw_route.alt_source_traversed_in_reverse.front());
            // Get all the coordinates for the computed route
            for (const PathData &path_data : raw_route.unpacked_alternative)
            {
                Descriptor<DataFacadeT>::current = Descriptor<DataFacadeT>::facade->GetCoordinateOfNode(path_data.node);
                Descriptor<DataFacadeT>::alternate_description_factory.AppendSegment(Descriptor<DataFacadeT>::current, path_data);
            }
            Descriptor<DataFacadeT>::alternate_description_factory.SetEndSegment(raw_route.segment_end_coordinates.back().target_phantom, raw_route.alt_source_traversed_in_reverse.back());
            Descriptor<DataFacadeT>::alternate_description_factory.Run(Descriptor<DataFacadeT>::facade, Descriptor<DataFacadeT>::config.zoom_level);

            if (Descriptor<DataFacadeT>::config.geometry)
            {
                std::string alternateGeometry;
                Descriptor<DataFacadeT>::alternate_description_factory.AppendEncodedPolylineStringEncoded(alternateGeometry);
                alternativeRoute.set_route_geometry(alternateGeometry);
            }
            // Generate instructions for each alternative (simulated here)
            if (Descriptor<DataFacadeT>::config.instructions)
            {
                BuildTextualDescription(raw_route.alternative_path_length,
                                        Descriptor<DataFacadeT>::alternate_description_factory,
                                        Descriptor<DataFacadeT>::alternative_path_segments,
                                        alternativeRoute);
            }

            Descriptor<DataFacadeT>::alternate_description_factory.BuildRouteSummary(
                Descriptor<DataFacadeT>::alternate_description_factory.entireLength, raw_route.alternative_path_length);

            protobufResponse::RouteSummary alternativeRouteSummary;

            alternativeRouteSummary.set_total_distance(Descriptor<DataFacadeT>::alternate_description_factory.summary.distance);
            alternativeRouteSummary.set_total_time(Descriptor<DataFacadeT>::alternate_description_factory.summary.duration);
            alternativeRouteSummary.set_start_point(Descriptor<DataFacadeT>::facade->GetEscapedNameForNameID(
                Descriptor<DataFacadeT>::alternate_description_factory.summary.source_name_id));
            alternativeRouteSummary.set_end_point(Descriptor<DataFacadeT>::facade->GetEscapedNameForNameID(
                Descriptor<DataFacadeT>::alternate_description_factory.summary.target_name_id));
            alternativeRoute.mutable_route_summary()->CopyFrom(routeSummary);

            std::vector<unsigned> const &alternate_leg_end_indices =
                Descriptor<DataFacadeT>::alternate_description_factory.GetViaIndices();
            for (unsigned v : alternate_leg_end_indices)
            {
                alternativeRoute.add_via_indices(v);
            }

            alternativeRoute.add_route_name(route_names.alternative_path_name_1);
            alternativeRoute.add_route_name(route_names.alternative_path_name_2);

            response.mutable_alternative_route()->CopyFrom(mainRoute);
        }

        mainRoute.add_route_name(route_names.shortest_path_name_1);
        mainRoute.add_route_name(route_names.shortest_path_name_2);

        protobufResponse::Hint hint;
        hint.set_check_sum(raw_route.check_sum);
        std::string res;
        for (const auto i : osrm::irange<std::size_t>(0, raw_route.segment_end_coordinates.size()))
        {
            EncodeObjectToBase64(raw_route.segment_end_coordinates[i].source_phantom, res);
            hint.add_location(res);
        }
        EncodeObjectToBase64(raw_route.segment_end_coordinates.back().target_phantom, res);
        hint.add_location(res);

        response.mutable_hint()->CopyFrom(hint);
        response.mutable_main_route()->CopyFrom(mainRoute);

        std::cout << response.DebugString() << std::endl;

        response.SerializeToString(&output);
        reply.content.insert(reply.content.end(), output.begin(), output.end());
    }
};
#endif // PBF_DESCRIPTOR_H
