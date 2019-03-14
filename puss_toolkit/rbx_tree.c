// rbx_tree.c

#include "rbx_tree.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

// list

static inline void __rbx_list_init(RBXNode *node) {
	node->prev = node;
	node->next = node;
}

static inline void __rbx_list_add(RBXNode *node, RBXNode *prev, RBXNode *next) {
	node->prev = prev;
	node->next = next;
	next->prev = node;
	prev->next = node;
}

static inline void __rbx_list_del(RBXNode *prev, RBXNode *next) {
	next->prev = prev;
	prev->next = next;
}

// brother list

static inline void __brother_list_init(RBXNode *node) {
	node->left = node;
	node->right = node;
}

static inline void __brother_list_add(RBXNode *node, RBXNode *left, RBXNode *right) {
	node->left = left;
	node->right = right;
	right->left = node;
	left->right = node;
}

static inline void __brother_list_del(RBXNode *left, RBXNode *right) {
	right->left = left;
	left->right = right;
}

// rbtree

static inline void __rbx_link_node(RBXNode* node, RBXNode* parent, RBXNode** link) {
	node->parent = parent;
	node->color = RBX_RED;
	node->left = node->right = 0;
	*link = node;
}

static inline void __rbx_rotate_left(RBXNode* node, RBXTree* tree) {
	RBXNode* right = node->right;

	if ((node->right = right->left))
		right->left->parent = node;
	right->left = node;

	if ((right->parent = node->parent)) {
		if (node==node->parent->left)
			node->parent->left = right;
		else
			node->parent->right = right;
	} else {
		tree->root = right;
	}
	node->parent = right;
}

static inline void __rbx_rotate_right(RBXNode* node, RBXTree* tree) {
	RBXNode* left = node->left;

	if ((node->left = left->right))
		left->right->parent = node;
	left->right = node;

	if ((left->parent = node->parent)) {
		if (node==node->parent->right)
			node->parent->right = left;
		else
			node->parent->left = left;
	} else {
		tree->root = left;
	}
	node->parent = left;
}

static inline void __rbx_erase_color(RBXNode* node, RBXNode* parent, RBXTree* tree) {
	RBXNode* other;
	RBXNode* tmp;
	while ((!node || node->color==RBX_BLACK) && node!=tree->root) {
		if (parent->left==node) {
			other = parent->right;
			if (other->color==RBX_RED) {
				other->color = RBX_BLACK;
				parent->color = RBX_RED;
				__rbx_rotate_left(parent, tree);
				other = parent->right;
			}
			if ((!other->left || other->left->color==RBX_BLACK) && (!other->right || other->right->color==RBX_BLACK)) {
				other->color = RBX_RED;
				node = parent;
				parent = node->parent;
			} else {
				if (!other->right || other->right->color==RBX_BLACK) {
					if ((tmp = other->left))
						tmp->color = RBX_BLACK;
					other->color = RBX_RED;
					__rbx_rotate_right(other, tree);
					other = parent->right;
				}
				other->color = parent->color;
				parent->color = RBX_BLACK;
				if (other->right)
					other->right->color = RBX_BLACK;
				__rbx_rotate_left(parent, tree);
				node = tree->root;
				break;
			}
		} else {
			other = parent->left;
			if (other->color==RBX_RED) {
				other->color = RBX_BLACK;
				parent->color = RBX_RED;
				__rbx_rotate_right(parent, tree);
				other = parent->left;
			}
			if ((!other->left || other->left->color==RBX_BLACK) && (!other->right || other->right->color==RBX_BLACK)) {
				other->color = RBX_RED;
				node = parent;
				parent = node->parent;
			} else {
				if (!other->left || other->left->color==RBX_BLACK) {
					if ((tmp = other->right))
						tmp->color = RBX_BLACK;
					other->color = RBX_RED;
					__rbx_rotate_left(other, tree);
					other = parent->left;
				}
				other->color = parent->color;
				parent->color = RBX_BLACK;
				if (other->left)
					other->left->color = RBX_BLACK;
				__rbx_rotate_right(parent, tree);
				node = tree->root;
				break;
			}
		}
	}
	if (node)
		node->color = RBX_BLACK;
}

static inline void __rbx_insert_color(RBXNode* node, RBXTree* tree) {
	RBXNode* parent;
	RBXNode* gparent;
	RBXNode * tmp;
	while ((parent = node->parent) && parent->color==RBX_RED) {
		gparent = parent->parent;

		if (parent==gparent->left) {
			tmp = gparent->right;
			if (tmp && tmp->color==RBX_RED) {
				tmp->color = RBX_BLACK;
				parent->color = RBX_BLACK;
				gparent->color = RBX_RED;
				node = gparent;
				continue;
			}

			if (parent->right==node) {
				__rbx_rotate_left(parent, tree);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			parent->color = RBX_BLACK;
			gparent->color = RBX_RED;
			__rbx_rotate_right(gparent, tree);
		} else {
			tmp = gparent->left;
			if (tmp && tmp->color==RBX_RED) {
				tmp->color = RBX_BLACK;
				parent->color = RBX_BLACK;
				gparent->color = RBX_RED;
				node = gparent;
				continue;
			}

			if (parent->left==node) {
				__rbx_rotate_right(parent, tree);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			parent->color = RBX_BLACK;
			gparent->color = RBX_RED;
			__rbx_rotate_left(gparent, tree);
		}
	}
	tree->root->color = RBX_BLACK;
}

static inline void __rbx_erase(RBXNode* node, RBXTree* tree) {
	RBXNode* child;
	RBXNode* parent;
	int color;

	if (!node->left) {
		child = node->right;
	} else if (!node->right) {
		child = node->left;
	} else {
		RBXNode * old = node, * left;

		node = node->right;
		while ((left = node->left))
			node = left;
		child = node->right;
		parent = node->parent;
		color = node->color;

		if (child)
			child->parent = parent;
		if (parent) {
			if (parent->left==node)
				parent->left = child;
			else
				parent->right = child;
		} else {
			tree->root = child;
		}
		if (node->parent==old)
			parent = node;
		node->parent = old->parent;
		node->color = old->color;
		node->right = old->right;
		node->left = old->left;

		if (old->parent) {
			if (old->parent->left==old)
				old->parent->left = node;
			else
				old->parent->right = node;
		} else {
			tree->root = node;
		}
		old->left->parent = node;
		if (old->right)
			old->right->parent = node;
		goto color_label;
	}

	parent = node->parent;
	color = node->color;

	if (child)
		child->parent = parent;
	if (parent) {
		if (parent->left==node)
			parent->left = child;
		else
			parent->right = child;
	} else {
		tree->root = child;
	}

color_label:
	if (color==RBX_BLACK)
		__rbx_erase_color(child, parent, tree);
}

// rbxtree

void rbx_init(RBXTree* tree) {
	memset(tree, 0, sizeof(RBXTree));
	__rbx_list_init(&(tree->list));
}

void rbx_insert(RBXNode* node, RBXNode* parent, RBXNode** link, RBXTree* tree) {
	assert( node && link && tree);
	assert( node!=parent );

	if( !parent ) {
		assert( tree->root==0 );
		__rbx_link_node(node, parent, link);
		__rbx_insert_color(node, tree);

		parent = &(tree->list);	// insert into list
		assert( parent->next==parent );
	} else if( *link ) {
		// append to brother list
		RBXNode* first = parent->next;
		assert( parent==*link );
		node->color = RBX_BROTHER;
		// node->parent = 0;
		if( first->color==RBX_BROTHER ) {
			parent = first->left;	// use parent as insert pos
			__brother_list_add(node, parent, first);
		} else {
			__brother_list_init(node);
		}
	} else {
		__rbx_link_node(node, parent, link);
		__rbx_insert_color(node, tree);

		if( link==&(parent->left) ) {
			parent = parent->prev;	// use parent as insert pos
		} else {
			RBXNode* first = parent->next;
			assert( link==&(parent->right) );
			if( first->color==RBX_BROTHER )
				parent = first->left;	// use parent as insert pos
		}
	}

	__rbx_list_add(node, parent, parent->next);
}

void rbx_erase(RBXNode* node, RBXTree* tree) {
	assert( node && tree );

	{
		RBXIter* iter = tree->iter;
		for( ; iter; iter=iter->next ) {
			if( iter->pos==node )
				iter->pos = node->prev;
		}
	}

	if( node->color==RBX_BROTHER ) {
		__brother_list_del(node->left, node->right);
	} else if( node->next->color==RBX_BROTHER ) {
		// replace node with brother list's first child
		RBXNode* parent = node->parent;
		RBXNode* first = node->next;
		__brother_list_del(first->left, first->right);

		first->color = node->color;
		first->parent = parent;
		first->left = node->left;
		first->right = node->right;
		if( parent ) {
			if( parent->left==node )
				parent->left = first;
			else
				parent->right = first;
		} else {
			assert( tree->root==node );
			tree->root = first;
		}
		if( first->left )
			first->left->parent = first;
		if( first->right )
			first->right->parent = first;
	} else {
		// remove from tree
		__rbx_erase(node, tree);
	}

	__rbx_list_del(node->prev, node->next);
}

void rbx_iter_tree(RBXIter* iter, RBXTree* tree) {
	RBXNode* node = &(tree->list);
	iter->pos = node;
	iter->tree = tree;
	iter->next = tree->iter;
	tree->iter = iter;
}

void rbx_iter_finish(RBXIter* iter) {
	RBXTree* rt = iter->tree;
	RBXIter* p;
	if (!rt)
		return;
	iter->tree = 0;

	if (rt->iter == iter) {
		rt->iter = iter->next;
		return;
	}

	for (p = rt->iter; p; p = p->next) {
		if (p->next == iter) {
			p->next = iter->next;
			return;
		}
	}
	assert(0 && "bad iter finish!");
}

