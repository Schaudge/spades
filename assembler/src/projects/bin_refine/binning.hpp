//***************************************************************************
//* Copyright (c) 2021 Saint Petersburg State University
//* All Rights Reserved
//* See file LICENSE for details.
//***************************************************************************

#pragma once

#include "assembly_graph/core/graph.hpp"
#include "pipeline/graph_pack.hpp"
#include <unordered_set>
#include <unordered_map>
#include <set>

namespace debruijn_graph {
class GraphPack;
}

namespace bin_stats {

class BinStats {
    static const std::string UNBINNED_ID;
 public:
    using BinLabel = std::string;
    using BinId = uint64_t;
    using ScaffoldName = std::string;
    using EdgeBinning = std::set<BinId>;
    using ScaffoldsPaths = std::unordered_map<ScaffoldName, std::unordered_set<debruijn_graph::EdgeId>>;

    static constexpr BinId UNBINNED = BinId(-1);
    
    explicit BinStats(const debruijn_graph::Graph& g)
            : graph_(g) {}

    /// binning file in .tsv format (NODE_{scaffold_id}_* -> bin_id); scaffolds_file in .fasta format
    void LoadBinning(const std::string& binning_file, const ScaffoldsPaths &scaffolds_paths);
    void WriteToBinningFile(const std::string& binning_file, const ScaffoldsPaths &scaffolds_paths);

    const debruijn_graph::Graph& graph() const { return graph_;  }

    const auto& edges_binning() const { return edges_binning_; }
    auto& edges_binning() { return edges_binning_; }

    const auto& unbinned_edges() const { return unbinned_edges_; }
    auto& unbinned_edges() { return unbinned_edges_; }

    const auto& bins() const { return bins_; }
    const auto& bin_labels() const { return bin_labels_; }

    friend std::ostream &operator<<(std::ostream &os, const BinStats &stats);
private:
    void ScaffoldsToEdges(const ScaffoldsPaths &scaffolds_paths);

    const debruijn_graph::Graph& graph_;

    std::unordered_map<ScaffoldName, BinId> scaffolds_binning_{};
    std::unordered_map<BinId, BinLabel> bin_labels_{};
    std::unordered_map<BinLabel, BinId> bins_{};
    std::unordered_map<debruijn_graph::EdgeId, EdgeBinning> edges_binning_{};
    std::unordered_set<debruijn_graph::EdgeId> unbinned_edges_{};

    BinId ChooseMajorBin(const std::vector<debruijn_graph::EdgeId>& path);
};
}