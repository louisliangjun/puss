// rbx_tree.h

#ifndef _INC_PUSSTOOLKIT_RBX_TREE_H_
#define _INC_PUSSTOOLKIT_RBX_TREE_H_

#include "puss_macros.h"

PUSS_DECLS_BEGIN

#define RBX_FREE	0
#define	RBX_RED		1
#define	RBX_BLACK	2
#define	RBX_BROTHER	3	// extend RB tree color for brother node

typedef struct _RBXNode		RBXNode;
typedef struct _RBXTree		RBXTree;
typedef struct _RBXLink		RBXLink;
typedef struct _RBXIter		RBXIter;

struct _RBXNode {
	RBXNode*	prev;
	RBXNode*	next;

	int			color;
	RBXNode*	parent;	// nouse when color==RBX_BROTHER
	RBXNode*	left;	// brother list prev when color==RBX_BROTHER
	RBXNode*	right;	// brother list next when color==RBX_BROTHER
};

struct _RBXTree {
	RBXNode		list;	// index list head
	RBXNode*	root;	// tree root node
	RBXIter*	iter;	// iterator list head
};

struct _RBXLink {
	RBXNode*	parent;
	RBXNode**	link;
	RBXTree*	tree;
};

struct _RBXIter {
	RBXNode*	pos;
	RBXTree*	tree;
	RBXIter*	next;
};

#define	RBX_INIT	{{0,0,0,0,0,0}, 0, 0}
#define	RBX_ENTRY(ptr, type, member)	((type *)((char *)(ptr)-(intptr_t)(&((type *)0)->member)))

void	rbx_init(RBXTree* tree);
void	rbx_insert(RBXNode* node, RBXNode* parent, RBXNode** link, RBXTree* tree);
void	rbx_erase(RBXNode* node, RBXTree* tree);
#define	rbx_insert_at(node, link_pos)	rbx_insert((node), (link_pos)->parent, (link_pos)->link, (link_pos)->tree)

void	rbx_iter_tree(RBXIter* iter, RBXTree* tree);
void	rbx_iter_finish(RBXIter* iter);

static inline RBXNode* rbx_iter_next(RBXIter* iter)	{
	RBXNode* next = iter->pos->next;
	if( (next==&(iter->tree->list)) )
		return 0;
	iter->pos = next;
	return next;
}

PUSS_DECLS_END

#endif//_INC_PUSSTOOLKIT_RBX_TREE_H_
