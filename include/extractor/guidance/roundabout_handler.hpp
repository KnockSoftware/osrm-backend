#ifndef OSRM_EXTRACTOR_GUIDANCE_ROUNDABOUT_HANDLER_HPP_
#define OSRM_EXTRACTOR_GUIDANCE_ROUNDABOUT_HANDLER_HPP_

#include "extractor/compressed_edge_container.hpp"
#include "extractor/guidance/coordinate_extractor.hpp"
#include "extractor/guidance/intersection.hpp"
#include "extractor/guidance/intersection_handler.hpp"
#include "extractor/guidance/is_through_street.hpp"
#include "extractor/guidance/roundabout_type.hpp"
#include "extractor/query_node.hpp"

#include "util/name_table.hpp"
#include "util/node_based_graph.hpp"
#include "util/typedefs.hpp"

#include <unordered_set>
#include <vector>

namespace osrm
{
namespace extractor
{
namespace guidance
{

namespace detail
{
struct RoundaboutFlags
{
    bool on_roundabout;
    bool can_enter;
    bool can_exit_separately;
};
} // namespace detail

// The roundabout handler processes all roundabout related instructions.
// It performs both the distinction between rotaries and roundabouts and
// assigns appropriate entry/exit instructions.
class RoundaboutHandler : public IntersectionHandler
{
  public:
    RoundaboutHandler(const util::NodeBasedDynamicGraph &node_based_graph,
                      const EdgeBasedNodeDataContainer &node_data_container,
                      const std::vector<util::Coordinate> &coordinates,
                      const extractor::CompressedEdgeContainer &compressed_geometries,
                      const RestrictionMap &node_restriction_map,
                      const std::unordered_set<NodeID> &barrier_nodes,
                      const guidance::TurnLanesIndexedArray &turn_lanes_data,
                      const util::NameTable &name_table,
                      const SuffixTable &street_name_suffix_table);

    ~RoundaboutHandler() override final = default;

    // check whether the handler can actually handle the intersection
    bool canProcess(const NodeID from_nid,
                    const EdgeID via_eid,
                    const Intersection &intersection) const override final;

    // process the intersection
    Intersection operator()(const NodeID from_nid,
                            const EdgeID via_eid,
                            Intersection intersection) const override final;

  private:
    detail::RoundaboutFlags getRoundaboutFlags(const NodeID from_nid,
                                               const EdgeID via_eid,
                                               const Intersection &intersection) const;

    // decide whether we lookk at a roundabout or a rotary
    RoundaboutType getRoundaboutType(const NodeID nid) const;

    // TODO handle bike/walk cases that allow crossing a roundabout!
    // Processing of roundabouts
    // Produces instructions to enter/exit a roundabout or to stay on it.
    // Performs the distinction between roundabout and rotaries.
    Intersection handleRoundabouts(const RoundaboutType roundabout_type,
                                   const EdgeID via_edge,
                                   const bool on_roundabout,
                                   const bool can_exit_roundabout,
                                   Intersection intersection) const;

    bool
    qualifiesAsRoundaboutIntersection(const std::unordered_set<NodeID> &roundabout_nodes) const;

    const CoordinateExtractor coordinate_extractor;
};

} // namespace guidance
} // namespace extractor
} // namespace osrm

#endif /*OSRM_EXTRACTOR_GUIDANCE_ROUNDABOUT_HANDLER_HPP_*/
