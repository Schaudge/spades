#pragma once


#include <cursor.hpp>
#include "aa.hpp"


template <typename T>
std::vector<std::vector<T>> expand(const std::vector<std::vector<T>> &vv) {
  std::vector<std::vector<T>> result;

  for (const auto &v : vv) {
    for (const auto &n : v.back().next()) {
      auto nw = v;
      nw.push_back(n);
      result.push_back(nw);
    }
  }

  return result;
}


template <class GraphCursor>
class AAGraphCursor {
  using This = AAGraphCursor<GraphCursor>;
 public:
  char letter() const {
    return to_one_letter(to_aa(c0_.letter(), c1_.letter(), c2_.letter()));
  }

  AAGraphCursor() = default;
  AAGraphCursor(const GraphCursor &c0, const GraphCursor &c1, const GraphCursor &c2) : c0_{c0}, c1_{c1}, c2_{c2} {}
  ~AAGraphCursor() noexcept = default;
  AAGraphCursor(const This&) = default;
  AAGraphCursor(This&&) = default;
  This & operator=(This&&) = default;
  This & operator=(const This&) = default;

  bool operator==(const AAGraphCursor &other) const {
    return c0_ == other.c0_ && c1_ == other.c1_ && c2_ == other.c2_;
  }

  bool is_empty() const {
    return c0_.is_empty() || c1_.is_empty() || c2_.is_empty();
  }

  std::vector<This> prev() const;

  static std::vector<This> from_bases(const std::vector<GraphCursor> &cursors) {
    std::vector<std::vector<GraphCursor>> nexts;
    for (const auto &cursor : cursors) {
      nexts.push_back({cursor});
    }

    for (size_t i = 0; i < 2; ++i) {
      nexts = expand(nexts);
    }

    std::vector<This> result;
    for (const auto &n : nexts) {
      assert(n.size() == 3);
      result.emplace_back(n[0], n[1], n[2]);
    }

    return result;
  }

  std::vector<This> next() const {
    return from_bases(c2_.next());
  }

 private:
  GraphCursor c0_, c1_, c2_;
  friend struct std::hash<AAGraphCursor<GraphCursor>>;
};

template <class GraphCursor>
auto make_aa_cursors(const std::vector<GraphCursor> &cursors) {
  return AAGraphCursor<GraphCursor>::from_bases(cursors);
}

template <class GraphCursor>
auto make_aa_cursors(const GraphCursor &cursor) {
  return make_aa_cursors({cursor});
}

namespace std {
template <class GraphCursor>
struct hash<AAGraphCursor<GraphCursor>> {
  std::size_t operator()(const AAGraphCursor<GraphCursor> &p) const {
    return std::hash<size_t>()(hash_size_t_pair(p.c0_, hash_size_t_pair(p.c1_, p.c2_)));  // TODO implement a proper hash for tuples
  }
};
}  // namespace std