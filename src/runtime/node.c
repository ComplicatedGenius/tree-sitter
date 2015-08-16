#include <stdbool.h>
#include "runtime/node.h"
#include "runtime/length.h"
#include "runtime/tree.h"
#include "runtime/document.h"

TSNode ts_node_make(const TSTree *tree, TSLength position) {
  return (TSNode){.data = tree, .position = position };
}

static inline const TSTree *get_tree(TSNode this) {
  return this.data;
}

TSLength ts_node_pos(TSNode this) {
  return ts_length_add(this.position, get_tree(this)->padding);
}

TSLength ts_node_size(TSNode this) {
  return get_tree(this)->size;
}

bool ts_node_eq(TSNode this, TSNode other) {
  return ts_tree_eq(get_tree(this), get_tree(other)) &&
         ts_length_eq(this.position, other.position);
}

const char *ts_node_name(TSNode this, const TSDocument *document) {
  return document->parser.language->symbol_names[get_tree(this)->symbol];
}

const char *ts_node_string(TSNode this, const TSDocument *document) {
  return ts_tree_string(get_tree(this), document->parser.language->symbol_names);
}

TSNode ts_node_parent(TSNode this) {
  TSLength position = this.position;
  const TSTree *tree = get_tree(this);

  do {
    TSTree *parent = tree->context.parent;
    if (!parent)
      return ts_node_null();

    for (size_t i = 0; i < parent->child_count; i++) {
      TSTree *child = parent->children[i];
      if (child == tree)
        break;
      position = ts_length_sub(position, ts_tree_total_size(child));
    }

    tree = parent;
  } while (!ts_tree_is_visible(tree));

  return ts_node_make(tree, position);
}

TSNode ts_node_prev_sibling(TSNode this) {
  const TSTree *tree = get_tree(this);
  TSLength position = this.position;
  do {
    TSTree *parent = tree->context.parent;
    if (!parent)
      break;

    for (size_t i = tree->context.index - 1; i + 1 > 0; i--) {
      const TSTree *child = parent->children[i];
      position = ts_length_sub(position, ts_tree_total_size(child));
      if (ts_tree_is_visible(child))
        return ts_node_make(child, position);
      if (child->visible_child_count > 0)
        return ts_node_child(ts_node_make(child, position), child->visible_child_count - 1);
    }

    tree = parent;
  } while (!ts_tree_is_visible(tree));

  return ts_node_null();
}

TSNode ts_node_next_sibling(TSNode this) {
  const TSTree *tree = get_tree(this);
  TSLength position = this.position;
  do {
    TSTree *parent = tree->context.parent;
    if (!parent)
      break;

    for (size_t i = tree->context.index + 1; i < parent->child_count; i++) {
      const TSTree *child = parent->children[i];
      position = ts_length_add(position, ts_tree_total_size(parent->children[i - 1]));
      if (ts_tree_is_visible(child))
        return ts_node_make(child, position);
      if (child->visible_child_count > 0)
        return ts_node_child(ts_node_make(child, position), 0);
    }

    tree = parent;
  } while (!ts_tree_is_visible(tree));

  return ts_node_null();
}

size_t ts_node_child_count(TSNode this) {
  return get_tree(this)->visible_child_count;
}

TSNode ts_node_child(TSNode this, size_t child_index) {
  TSNode node = this;
  bool did_descend = true;
  while (did_descend) {
    did_descend = false;
    const TSTree *tree = get_tree(node);
    TSLength position = node.position;

    size_t index = 0;
    for (size_t i = 0; i < tree->child_count; i++) {
      TSTree *child = tree->children[i];
      if (ts_tree_is_visible(child)) {
        if (index == child_index)
          return ts_node_make(child, position);
        index++;
      } else {
        size_t grandchild_index = child_index - index;
        if (grandchild_index < child->visible_child_count) {
          did_descend = true;
          node = ts_node_make(child, position);
          child_index = grandchild_index;
        }
        index += child->visible_child_count;
      }
      position = ts_length_add(position, ts_tree_total_size(child));
    }
  }

  return ts_node_null();
}

TSNode ts_node_find_for_range(TSNode this, size_t min, size_t max) {
  TSNode node = this, last_visible_node = this;
  bool did_descend = true;
  while (did_descend) {
    did_descend = false;
    const TSTree *tree = get_tree(node);
    TSLength position = node.position;

    for (size_t i = 0; i < tree->child_count; i++) {
      const TSTree *child = tree->children[i];
      if (position.chars + child->padding.chars > min)
        break;
      if (position.chars + child->padding.chars + child->size.chars > max) {
        node = ts_node_make(child, position);
        if (ts_tree_is_visible(child))
          last_visible_node = node;
        did_descend = true;
      }
      position = ts_length_add(position, ts_tree_total_size(child));
    }
  }

  return last_visible_node;
}

TSNode ts_node_find_for_pos(TSNode this, size_t position) {
  return ts_node_find_for_range(this, position, position);
}
