// rbx_tree.inl

#include <assert.h>

#define _RBX_TREE_FULL_CHECK

#define RBX_FREE	0
#define	RBX_RED		1
#define	RBX_BLACK	2
#define	RBX_BROTHER	3	// extend RB tree color for brother node

typedef struct _RBXNode		RBXNode;
typedef struct _RBXTree		RBXTree;
typedef struct _RBXIter		RBXIter;
typedef struct _RBXLink		RBXLink;

struct _RBXNode {
	RBXNode*		prev;
	RBXNode*		next;

	RBXNode*		left;
	RBXNode*		right;
	RBXNode*		parent;
	int				color;
};

struct _RBXTree {
	RBXNode			list;	// index list head
	RBXNode*		root;	// tree root node
	RBXIter*		iter;	// iterator list head
};

struct _RBXIter {
	RBXNode*		pos;
	RBXTree*		tree;
	RBXIter*		next;
};

struct _RBXLink {
	RBXNode*		parent;
	RBXNode**		link;
	RBXTree*		tree;
};

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

static void rbx_init(RBXTree* tree) {
	memset(tree, 0, sizeof(RBXTree));
	__rbx_list_init(&(tree->list));
}

#define __rbx_has_brother(node) ((node)->next->color==RBX_BROTHER)

static void rbx_insert(RBXNode* node, RBXNode* parent, RBXNode** link, RBXTree* tree) {
	assert( node && link && tree);
	assert( node!=parent );

	if( !parent ) {
		assert( tree->root==0 );
		__rbx_link_node(node, parent, link);
		__rbx_insert_color(node, tree);

		parent = &(tree->list);	// insert into list
		assert( parent->next==parent );
	} else if( *link ) {
		// insert node into brother list
		assert( parent==*link );
		node->color = RBX_BROTHER;
		node->parent = 0;

		if( __rbx_has_brother(parent) ) {
			// append to brother list
			RBXNode* first = parent->next;
			RBXNode* last  = first->left;
			node->left = last;
			node->right = first;
			first->left = node;
			last->right = node;
			parent = last;	// use parent as insert pos
		} else {
			// borther list head
			node->left = node;
			node->right = node;
		}
	} else {
		__rbx_link_node(node, parent, link);
		__rbx_insert_color(node, tree);

		if( link==&(parent->left) ) {
			parent = parent->prev;	// use parent as insert pos

		} else {
			assert( link==&parent->right );
			if( __rbx_has_brother(parent) )
				parent = parent->next->left;	// use parent as insert pos
		}
	}

	__rbx_list_add(node, parent, parent->next);
}

static void rbx_erase(RBXNode* node, RBXTree* tree) {
	assert( node && tree );

	// update iterators
	{
		RBXIter* iter = tree->iter;
		for( ; iter; iter=iter->next ) {
			if( iter->pos==node )
				iter->pos = node->prev;
		}
	}

	if( node->color==RBX_BROTHER ) {
		// remove from brother list
		node->right->left = node->left;
		node->left->right = node->right;
	} else if( __rbx_has_brother(node) ) {
		// replace node with brother list's first child
		RBXNode* first = node->next;
		RBXNode* parent = node->parent;

		// remove first from brother list
		first->right->left = first->left;
		first->left->right = first->right;

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

// debug utils
// 
#ifdef _RBX_TREE_FULL_CHECK
	static int __rbx_dbg_check_node_in_tree(RBXNode* node, RBXTree* tree) {
		RBXNode* list = &(tree->list);
		RBXNode* p;
		for( p = list->next; p!=list; p=p->next ) {
			if( p==node )
				return 1;
		}
		return 0;
	}

	#define	__rbx_insert_check(node, parent, link, tree) do { \
				assert( !__rbx_dbg_check_node_in_tree((node), (tree)) ); \
				rbx_insert((node), (parent), (link), (tree)); \
			} while(0)

	#define	__rbx_erase_check(node, tree) do { \
				assert( __rbx_dbg_check_node_in_tree((node), (tree)) ); \
				rbx_erase((node), (tree)); \
			} while(0)
#else
	#define	__rbx_insert_check	rbx_insert
	#define	__rbx_erase_check	rbx_erase
#endif

// iterator
// 
static void rbx_iter_tree(RBXIter* iter, RBXTree* tree) {
	RBXNode* node = &(tree->list);
	iter->pos = node;
	iter->tree = tree;
	iter->next = tree->iter;
	tree->iter = iter;
}

static void rbx_iter_finish(RBXIter* iter) {
	RBXTree* rt = iter->tree;
	RBXIter* p;
	if( !rt )
		return;
	iter->tree = 0;

	if( rt->iter==iter ) {
		rt->iter = iter->next;
	} else {
		for( p=rt->iter; p; p=p->next ) {
			if( p->next==iter ) {
				p->next = iter->next;
				return;
			}
		}
		assert( 0 && "bad iter finish!" );
	}
}

static inline RBXNode* rbx_iter_next(RBXIter* iter)	{
	RBXNode* next = iter->pos->next;
	if( (next==&(iter->tree->list)) )
		return 0;
	iter->pos = next;
	return next;
}

#define	rbx_entry(ptr, type, member)	((type *)((char *)(ptr)-(intptr_t)(&((type *)0)->member)))

#define	rbx_insert_at(node, link_pos)	rbx_insert((node), (link_pos)->parent, (link_pos)->link, (link_pos)->tree)

