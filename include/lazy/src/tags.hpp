#ifndef LAZY_TAGS_HPP
#define LAZY_TAGS_HPP

#include <type_traits>


namespace lazy::tags{


// ============================================================================
// Operation tag types
// ============================================================================
// Tags are empty structs used as the first argument to `evaluate` / `eval_rule`
// to discriminate which arithmetic operation is being requested.  The hierarchy
// is: Tag <- specific arithmetic/comparison tags.
//            BOOL_TAG <- comparison-specific tags.

/// @brief Root tag base.  All operation tags inherit from this.
struct Tag{};

} // namespace lazy::tags

namespace lazy::traits{

template<typename Arg>
concept isTag = std::is_base_of_v<lazy::tags::Tag, std::decay_t<Arg>>;

} // namespace lazy::traits

#endif // LAZY_TAGS_HPP