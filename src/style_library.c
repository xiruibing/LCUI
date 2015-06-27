﻿/* ***************************************************************************
 * style_library.c -- style library module for LCUI.
 *
 * Copyright (C) 2012-2015 by Liu Chao <lc-soft@live.cn>
 *
 * This file is part of the LCUI project, and may only be used, modified, and
 * distributed under the terms of the GPLv2.
 *
 * (GPLv2 is abbreviation of GNU General Public License Version 2)
 *
 * By continuing to use, modify, or distribute this file you indicate that you
 * have read the license and understand and accept it fully.
 *
 * The LCUI project is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GPL v2 for more details.
 *
 * You should have received a copy of the GPLv2 along with this file. It is
 * usually in the LICENSE.TXT file, If not, see <http://www.gnu.org/licenses/>.
 * ****************************************************************************/

/* ****************************************************************************
 * style_library.c -- LCUI 的样式库模块。
 *
 * 版权所有 (C) 2015 归属于 刘超 <lc-soft@live.cn>
 *
 * 这个文件是LCUI项目的一部分，并且只可以根据GPLv2许可协议来使用、更改和发布。
 *
 * (GPLv2 是 GNU通用公共许可证第二版 的英文缩写)
 *
 * 继续使用、修改或发布本文件，表明您已经阅读并完全理解和接受这个许可协议。
 *
 * LCUI 项目是基于使用目的而加以散布的，但不负任何担保责任，甚至没有适销性或特
 * 定用途的隐含担保，详情请参照GPLv2许可协议。
 *
 * 您应已收到附随于本文件的GPLv2许可协议的副本，它通常在LICENSE.TXT文件中，如果
 * 没有，请查看：<http://www.gnu.org/licenses/>.
 * ****************************************************************************/

#include <LCUI_Build.h>
#include <LCUI/LCUI.h>
#include <LCUI/style_library.h>

#define MAX_NAME_LEN		128
#define MAX_NODE_DEPTH		32
#define MAX_SELECTOR_DEPTH	32

typedef struct StyleListNode {
	LCUI_StyleSheet style;		/**< 样式表 */
	LCUI_Selector selector;		/**< 作用范围 */
} StyleListNode;

typedef struct StyleTreeNode {
	char name[MAX_NAME_LEN];	/**< 作用对象的名称 */
	LinkedList styles;		/**< 其它样式表 */
} StyleTreeNode;

static struct {
	LCUI_BOOL is_inited;
	LCUI_RBTree style;		/**< 样式树 */
} style_library;

/** 获取指针数组的长度 */
static size_t ptrslen( const char **list )
{
	const char **p = list;
	while( *p ) {
		p++;
	}
	return p - list;
}

static int CompareName( void *data, const void *keydata )
{
	return strcmp( ((StyleTreeNode*)data)->name, (const char*)keydata );
}

/** 删除选择器 */
void DeleteSelector( LCUI_Selector *selector )
{
	LCUI_SelectorNode *node_ptr;
	for( node_ptr=*selector; node_ptr; ++node_ptr ) {
		if( (*node_ptr)->name ) {
			free( (*node_ptr)->name );
			(*node_ptr)->name = NULL;
		}
		if( (*node_ptr)->class_name ) {
			free( (*node_ptr)->class_name );
			(*node_ptr)->class_name = NULL;
		}
		if( (*node_ptr)->id ) {
			free( (*node_ptr)->id );
			(*node_ptr)->id = NULL;
		}
		if( (*node_ptr)->pseudo_class_name ) {
			free( (*node_ptr)->pseudo_class_name );
			(*node_ptr)->pseudo_class_name = NULL;
		}
	}
	free( *selector );
	*selector = NULL;
}

/** 删除样式表 */
void DeleteStyleSheet( LCUI_StyleSheet *ss )
{
	free( *ss );
	*ss = NULL;
}

static void DestroyStyleListNode( void *data )
{
	StyleListNode *node = (StyleListNode*)data;
	DeleteStyleSheet( &node->style );
	DeleteSelector( &node->selector );
}

static void DestroyStyleTreeNode( void *data )
{
	LinkedList_Destroy( &((StyleTreeNode*)data)->styles );
}

/** 合并两个样式表 */
static void MergeStyleSheet( LCUI_StyleSheet dest, LCUI_StyleSheet source )
{
	int i;
	for( i=0; i<STYLE_KEY_TOTAL; ++i ) {
		if( source[i].is_valid && !dest[i].is_valid ) {
			dest[i] = source[i];
		}
	}
}

/** 覆盖样式表 */
static void ReplaceStyleSheet( LCUI_StyleSheet dest, LCUI_StyleSheet source )
{
	int i;
	for( i=0; i<STYLE_KEY_TOTAL; ++i ) {
		if( source[i].is_valid ) {
			dest[i] = source[i];
			dest[i].is_valid = TRUE;
		}
	}
}

/** 初始化 */
void LCUI_InitStyleLibrary( void ) 
{
	RBTree_Init( &style_library.style );
	RBTree_OnJudge( &style_library.style, CompareName );
	RBTree_OnDestroy( &style_library.style, DestroyStyleTreeNode );
	style_library.is_inited = TRUE;
}

/** 销毁，释放资源 */
void LCUI_ExitStyleLibrary( void ) 
{
	RBTree_Destroy( &style_library.style );
	style_library.is_inited = FALSE;
}

LCUI_StyleSheet StyleSheet( void )
{
	LCUI_StyleSheet ss;
	ss = (LCUI_StyleSheet)calloc( STYLE_KEY_TOTAL, sizeof(LCUI_Style) );
	return ss;
}

static int SaveSelectorNode( LCUI_SelectorNode node, const char *name, char type )
{

	switch( type ) {
	case 0:
		if( node->name ) {
			return -1;
		}
		node->name = strdup( name );
		break;
	case ':':
		if( node->pseudo_class_name ) {
			return -2;
		}
		node->pseudo_class_name = strdup( name );
		break;
	case '.':
		if( node->class_name ) {
			return -3;
		}
		node->class_name = strdup( name );
		break;
	case '#':
		if( node->id ) {
			return -4;
		}
		node->id = strdup( name );
		break;
	default: break;
	}
	return 0;
}

/** 根据字符串内容生成相应的选择器 */
LCUI_Selector Selector( const char *selector )
{
	int ni, si;
	const char *p;
	char type, name[MAX_NAME_LEN];
	size_t size;
	LCUI_BOOL is_saving = FALSE;
	LCUI_SelectorNode node = NULL;
	LCUI_Selector s; 

	size = sizeof(LCUI_SelectorNode)*MAX_SELECTOR_DEPTH;
	s = (LCUI_Selector)malloc( size );
	for( ni = 0, si = 0, p = selector; *p; ++p ) {
		if( node == NULL && is_saving ) {
			size = sizeof( struct LCUI_SelectorNodeRec_ );
			node = (LCUI_SelectorNode)malloc( size );
			if( si >= MAX_SELECTOR_DEPTH ) {
				_DEBUG_MSG( "%s: selector node list is too long.",
					    selector, *p, p - selector - ni );
				return NULL;
			}
			s[si++] = node;
		}
		if( *p == ':' || *p == '.' || *p == '#' ) {
			if( is_saving ) {
				if( SaveSelectorNode( node, name, type ) != 0 ) {
					_DEBUG_MSG( "%s: invalid selector node at %d\n",
						    selector, *p, p - selector - ni );
					return NULL;
				}
				ni = 0;
			}
			is_saving = TRUE;
			type = *p;
			continue;
		}
		if( *p == ' ' ) {
			if( SaveSelectorNode( node, name, type ) != 0 ) {
				_DEBUG_MSG( "%s: invalid selector node at %d\n",
					    selector, *p, p - selector - ni );
				return NULL;
			}
			ni = 0;
			node = NULL;
			continue;
		}
		if( (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
		 || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' ) {
			if( !is_saving ) {
				type = 0;
				is_saving = TRUE;
			}
			name[ni++] = *p;
			name[ni] = 0;
			continue;
		}
		_DEBUG_MSG( "%s: unknown char %02x at %d\n",
			    selector, *p, p - selector );
		return NULL;
	}
	return s;
}

static int mystrcmp( const char *str1, const char *str2 )
{
	if( str1 == str2 ) {
		return 0;
	}
	if( str1 == NULL || str2 == NULL ) {
		return -1;
	}
	return strcmp( str1, str2 );
}

/** 判断两个选择器是否相等 */
LCUI_BOOL SelectorIsEqual( LCUI_Selector s1, LCUI_Selector s2 )
{
	LCUI_SelectorNode *sn1_ptr = s1, *sn2_ptr = s2;
	for( ; sn1_ptr && sn2_ptr; ++sn1_ptr,++sn2_ptr ) {
		if( mystrcmp((*sn1_ptr)->name, (*sn2_ptr)->name) ) {
			return FALSE;
		}
		if( mystrcmp((*sn1_ptr)->class_name,
			(*sn2_ptr)->class_name) ) {
			return FALSE;
		}
		if( mystrcmp((*sn1_ptr)->pseudo_class_name, 
			(*sn2_ptr)->pseudo_class_name) ) {
			return FALSE;
		}
		if( mystrcmp((*sn1_ptr)->id, (*sn2_ptr)->id) ) {
			return FALSE;
		}
	}
	if( sn1_ptr == sn2_ptr ) {
		return TRUE;
	}
	return FALSE;
}

static LCUI_StyleSheet SelectStyleSheetByName( LCUI_Selector selector,
					       const char *name )
{
	int i, n;
	LCUI_RBTreeNode *node;
	StyleTreeNode *stn;
	StyleListNode *sln;
	LCUI_SelectorNode sn;

	node = RBTree_CustomSearch( &style_library.style, (const void*)name );
	if( !node ) {
		stn = (StyleTreeNode*)malloc(sizeof(StyleTreeNode));
		strncpy( stn->name, name, MAX_NAME_LEN );
		LinkedList_Init( &stn->styles, sizeof(StyleListNode) );
		LinkedList_SetDataNeedFree( &stn->styles, TRUE );
		LinkedList_SetDestroyFunc( &stn->styles, DestroyStyleTreeNode );
		node = RBTree_CustomInsert( &style_library.style, 
					    (const void*)name, stn );
	}
	stn = (StyleTreeNode*)node->data;
	LinkedList_Goto( &stn->styles, 0 );
	while( LinkedList_IsAtEnd( &stn->styles ) ) {
		sln = (StyleListNode*)LinkedList_Get( &stn->styles );
		if( SelectorIsEqual(sln->selector, selector) ) {
			return sln->style;
		}
		LinkedList_ToNext( &stn->styles );
	}
	sln = (StyleListNode*)malloc(sizeof(StyleListNode));
	sln->style = StyleSheet();
	for( n=0; selector[n]; ++n );
	sln->selector = (LCUI_Selector)malloc( sizeof(LCUI_SelectorNode*)*n );
	for( i=0, n-=1; i<n; ++i ) {
		sn = (LCUI_SelectorNode)malloc( sizeof(struct LCUI_SelectorNodeRec_) );
		sn->name = NULL;
		sn->id = NULL;
		sn->class_name = NULL;
		sn->pseudo_class_name = NULL;
		if( selector[i]->name ) {
			sn->name = strdup( selector[i]->name );
		}
		if( selector[i]->id ) {
			sn->id = strdup( selector[i]->id );
		}
		if( selector[i]->class_name ) {
			sn->class_name = strdup( selector[i]->class_name );
		}
		if( selector[i]->pseudo_class_name ) {
			sn->pseudo_class_name = strdup( selector[i]->pseudo_class_name );
		}
		sln->selector[i] = sn;
	}
	sln->selector[n] = NULL;
	LinkedList_Append( &stn->styles, sln );
	return sln->style;
}

static LCUI_StyleSheet SelectStyleSheet( LCUI_Selector selector )
{
	int depth;
	char fullname[MAX_NAME_LEN];
	LCUI_SelectorNode sn;
	
	for( depth = 0; selector[depth]; ++depth );
	sn = selector[depth-1];
	/* 优先级：伪类 > 类 > ID > 名称 */
	if( sn->pseudo_class_name ) {
		fullname[0] = ':';
		strncpy( fullname + 1, sn->pseudo_class_name, MAX_NAME_LEN-1 );
		return SelectStyleSheetByName( selector, fullname );
	}
	if( sn->class_name ) {
		fullname[0] = '.';
		strncpy( fullname + 1, sn->class_name, MAX_NAME_LEN-1 );
		return SelectStyleSheetByName( selector, fullname );
	}
	if( sn->id ) {
		fullname[0] = '#';
		strncpy( fullname + 1, sn->id, MAX_NAME_LEN-1 );
		return SelectStyleSheetByName( selector, fullname );
	}
	if( sn->name ) {
		return SelectStyleSheetByName( selector, fullname );
	}
	return NULL;
}

/** 添加样式表 */
int LCUI_PutStyle( LCUI_Selector selector, LCUI_StyleSheet in_ss )
{
	LCUI_StyleSheet ss;
	ss = SelectStyleSheet( selector );
	if( ss ) {
		ReplaceStyleSheet( ss, in_ss );
	}
	return 0;
}

/** 匹配元素路径与样式结点路径 */
LCUI_BOOL IsMatchPath( LCUI_Object *obj_path, LCUI_Selector selector )
{
	int i, n;
	LCUI_SelectorNode *sn_ptr = selector;
	LCUI_Object *obj_ptr = obj_path, obj;

	while( ++obj_ptr && sn_ptr ) {
		obj = *obj_ptr;
		if( (*sn_ptr)->id ) {
			if( strcmp(obj->id, (*sn_ptr)->id) ) {
				continue;
			}
		}
		if( (*sn_ptr)->name ) {
			if( strcmp(obj->name, (*sn_ptr)->name) ) {
				continue;
			}
		}
		if( (*sn_ptr)->class_name ) {
			n = ptrslen( obj->classes );
			for( i = 0; i < n; ++i ) {
				if( strcmp(obj->classes[i],
					(*sn_ptr)->class_name) ) {
					break;
				}
			}
			if( i >= n ) {
				continue;
			}
		}
		if( (*sn_ptr)->pseudo_class_name ) {
			n = ptrslen( obj->pseudo_classes );
			for( i = 0; i < n; ++i ) {
				if( strcmp(obj->pseudo_classes[i],
					(*sn_ptr)->pseudo_class_name) ) {
					break;
				}
			}
			if( i >= n ) {
				continue;
			}
		}
		++sn_ptr;
	}
	if( sn_ptr == NULL ) {
		return TRUE;
	}
	return FALSE;
}

static int FindStyleNodeByName( const char *name, LCUI_Object obj, 
				LinkedList *list )
{
	LCUI_RBTreeNode *node;
	StyleListNode *sln;
	LinkedList *styles;
	LCUI_Object s, objs[MAX_SELECTOR_DEPTH];
	int i, n, count;

	node = RBTree_CustomSearch( &style_library.style, (const void*)name );
	if( !node ) {
		return 0;
	}
	for( n = 0, s = obj; s; ++n, s = s->parent );
	if( n >= MAX_SELECTOR_DEPTH ) {
		return -1;
	}
	if( n == 0 ) {
		objs[0] = NULL;
	} else {
		n -= 1;
		objs[n] = NULL;
		s = obj->parent;
		while( --n >= 0 ) {
			objs[n] = s;
			s = s->parent;
		}
	}
	styles = &((StyleTreeNode*)node->data)->styles;
	n = LinkedList_GetTotal( styles );
	LinkedList_Goto( styles, 0 );
	for( count=0,i=0; i<n; ++i ) {
		sln = (StyleListNode*)LinkedList_Get( styles );
		/* 如果当前元素在该样式结点的作用范围内 */
		if( IsMatchPath(objs, sln->selector) ) {
			LinkedList_Append( list, sln->style );
			++count;
		}
	}
	return count;
}

static int FindStyleNode( LCUI_Object obj, LinkedList *list )
{
	int i, count = 0;
	char fullname[MAX_NAME_LEN];

	i = ptrslen( obj->classes );
	/* 记录类选择器匹配的样式表 */
	while( --i >= 0 ) {
		fullname[0] = '.';
		strncpy( fullname + 1, obj->classes[i], MAX_NAME_LEN-1 );
		count += FindStyleNodeByName( fullname, obj, list );
	}
	/* 记录ID选择器匹配的样式表 */
	if( obj->id ) {
		fullname[0] = '#';
		strncpy( fullname + 1, obj->id, MAX_NAME_LEN - 1 );
		count += FindStyleNodeByName( fullname, obj, list );
	}
	/* 记录名称选择器匹配的样式表 */
	if( obj->name ) {
		count += FindStyleNodeByName( obj->name, obj, list );
	}
	return count;
}

/** 获取样式表 */
int LCUI_GetStyle( LCUI_Object obj, LCUI_StyleSheet out_ss )
{
	int i, n;
	LCUI_StyleSheet ss;
	LinkedList list;

	LinkedList_Init( &list, sizeof(LCUI_StyleSheet) );
	LinkedList_SetDataNeedFree( &list, FALSE );
	n = FindStyleNode( obj, &list );
	LinkedList_Goto( &list, 0 );
	for( i=0; i<n; ++i ) {
		ss = (LCUI_StyleSheet)LinkedList_Get( &list );
		MergeStyleSheet( out_ss, ss );
	}
	LinkedList_Destroy( &list );
	return 0;
}