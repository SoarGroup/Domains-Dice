/*************************************************************************
 *
 * file:  epmem.c
 *
 * Routines for Soar's episodic memory module (added in 2002 by :AMN:)
 *
 *
 * Copyright (c) 1995-2002 Carnegie Mellon University,
 *                         The Regents of the University of Michigan,
 *                         University of Southern California/Information
 *                         Sciences Institute.  All rights reserved.
 *
 * The Soar consortium proclaims this software is in the public domain, and
 * is made available AS IS.  Carnegie Mellon University, The University of 
 * Michigan, and The University of Southern California/Information Sciences 
 * Institute make no warranties about the software or its performance,
 * implied or otherwise.
 * =======================================================================
 */

/*
 *  %%%TODO:
 *  1.  Add my own MEM_USAGE constant?
 *  2.  Put global variables into soar_agent (e.g., current_agent())
 *  3.  Allow user to configure the global variables on the command line
 */

#include "soarkernel.h"

#ifdef AMN_EP_MEM

//defined in soar_core_utils.c
extern wme ** get_augs_of_id( Symbol *id, tc_number tc, int *num_attr );

/* EpMem constants
   
   num_active_wmes - epmem uses the n most active wmes to decide whether to
                     record a new memory.  This is n.
   num_wmes_changed - number of wmes in the current list that must be
                      different from the previous list to trigger
                      a new memory.
   num_cue_wmes - If using an activation-based cue, this value indicates
                  how many wmes (i.e.., the n most active) will be used
                  to construct the cue.

   %%%TODO:  Made the above values command line configurable

   These tree constants define apply production types for a memory
   EPMEM_ADD    - 
   EPMEM_CHANGE 5
   EPMEM_REMOVE 6
*/

#define num_active_wmes 1
#define num_wmes_changed 1
#define childlist_init_size 8
#define wmelist_init_size 128   // Initial size for a wmelist array
#define memories_init_size 512  // Initial size for the global memories array

#define EPMEM_ADD 4
#define EPMEM_CHANGE 5
#define EPMEM_REMOVE 6


/*======================================================================
 * Data structures
 *----------------------------------------------------------------------
 */

/*

   wmetree - Used to build a tree representation of a state
             which is used to construct an episodic memory

             id   - every WME in the tree has a unique id.  I can't
                    use the timetag because multiple WMEs may have the same
                    value.
             attr - the string representing the WME attribute associated
                    with this tree node
             attr - the string representing the WME value associated
                    with this tree node (*only* if this is a leaf node)
             is_leaf - TRUE means that this node has no children
             children - children of the current node
             cap_children - the size of the children array
             num_children - number of filled cells in the array
             parent - parent of the current node.
             next/prev - dll of all nodes in the tree to allow iterative
                         implementation of several algorithms
             in_mem - whether this "wme" is in the current memory
                      
*/


typedef struct wmetree_struct
{
    char *attr;
    union
    {
        char *strval;
        long intval;
        float floatval;
    } val;
    int val_type;
    
    struct wmetree_struct **children;
    int cap_children;
    int num_children;
    struct wmetree_struct *parent; // %%%make this an array later?
    bool in_mem;
    struct wme_struct *assoc_wme;
} wmetree;

/*
 * wmelist - A list of all the WMEs from a wmetree.  I use this struct
 *           as the high level handle to the episodic memory.  Each
 *           individual struct is a single Soar state.  A string of
 *           them is a snapshot of working memory at a particular
 *           instant.
 *           nodes - an array of pointers to nodes in the wmetree
 *           cap_nodes  - the size of the array
 *           num_nodes - the number of cells in the array that are filled
 *           next - pointer to the next wmelist
 *
 */
typedef struct wmelist_struct
{
    wmetree **nodes;
    long cap_nodes;
    long num_nodes;
    struct wmelist_struct *next;
} wmelist;

/*======================================================================
 * EpMem globals
 *----------------------------------------------------------------------
 */

/*

   g_current_active_wmes  - the n most active wmes in working memory
   g_previous_active_wmes - the n most active wmes in working memory
                            when the last memory was recorded
   g_wmetree              - The head of a giant wmetree used to represent all
                            the states that that agent has seen so far.  This is,
                            in effect, the episodic store of the agent.
                            
*/

//%%%FIXME:  move these globals to current_agent()
wme **g_current_active_wmes;
wme **g_previous_active_wmes;
wmetree g_wmetree;
wmelist **g_memories;
int g_cap_memories;
int g_num_memories;
Symbol *g_epmem_header;
Symbol * g_epmem_query;
Symbol * g_epmem_retrieved;



/* EpMem macros

   IS_LEAF_WME(w) - is w a leaf WME? (i.e., value is not an identifier)
   
*/
#define IS_LEAF_WME(w) ((w)->value->common.symbol_type != IDENTIFIER_SYMBOL_TYPE)


/* ===================================================================
   make_wmetree_node

   Creates a new wmetree node based upon a given wme.  If no WME is
   given (w == NULL) then an empty node is allocated.  The caller is
   responsible for setting the parent pointer.
   
   Created: 09 Jan 2004
   =================================================================== */
wmetree *make_wmetree_node(wme *w)
{
    wmetree *node;

    node = allocate_memory(sizeof(wmetree), MISCELLANEOUS_MEM_USAGE);
    node->attr = NULL;
    node->val.intval = 0;
    node->val_type = IDENTIFIER_SYMBOL_TYPE;
    node->children = NULL;
    node->cap_children = 0;
    node->num_children = 0;
    node->parent = NULL;
    node->in_mem = FALSE;
    node->assoc_wme = NULL;

    if (w == NULL) return node;
    
    node->attr = allocate_memory(sizeof(char)*strlen(w->attr->sc.name) + 1,
                                MISCELLANEOUS_MEM_USAGE);
    strcpy(node->attr, w->attr->sc.name);
    
    switch(w->value->common.symbol_type)
    {
        case SYM_CONSTANT_SYMBOL_TYPE:
            node->val_type = SYM_CONSTANT_SYMBOL_TYPE;
            node->val.strval =
                allocate_memory(sizeof(char)*strlen(w->value->sc.name) + 1,
                                MISCELLANEOUS_MEM_USAGE);
            strcpy(node->val.strval, w->value->sc.name);
            break;

        case INT_CONSTANT_SYMBOL_TYPE:
            node->val_type = INT_CONSTANT_SYMBOL_TYPE;
            node->val.intval = w->value->ic.value;
            break;

        case FLOAT_CONSTANT_SYMBOL_TYPE:
            node->val_type = FLOAT_CONSTANT_SYMBOL_TYPE;
            node->val.floatval = w->value->fc.value;
            break;

        default:
            node->val_type = IDENTIFIER_SYMBOL_TYPE;
            node->val.intval = 0;
            break;
    }

    return node;
}//make_wmetree_node

/* ===================================================================
   destroy_wmetree (+ dw_helper)       *RECURSIVE*

   Deallocate an entire wmetree.

   Created 10 Nov 2002
   =================================================================== */
void dw_helper(wmetree *tree)
{
    int i;

    if (tree->attr != NULL)
    {
        free_memory(tree->attr, MISCELLANEOUS_MEM_USAGE);
    }
    
    if (tree->val_type == SYM_CONSTANT_SYMBOL_TYPE)
    {
        free_memory(tree->val.strval, MISCELLANEOUS_MEM_USAGE);
    }
    
    if (tree->num_children == 0)
    {
        return;
    }
    
    for(i = 0; i < tree->num_children; i++)
    {
        //recursive call
        dw_helper(tree->children[i]);
        free_memory(tree->children[i], MISCELLANEOUS_MEM_USAGE);
    }

    free_memory(tree->children, MISCELLANEOUS_MEM_USAGE);
}//dw_helper

void destroy_wmetree(wmetree *tree)
{
    dw_helper(tree);

    free_memory(tree, MISCELLANEOUS_MEM_USAGE);
}//destroy_wmetree

/* ===================================================================
   make_wmelist

   Allocates and initializes a new, empty wmelist and returns a pointer
   to the caller.
   
   Created: 13 Jan 2004
   =================================================================== */
wmelist *make_wmelist()
{
    wmelist *wl;

    wl = (wmelist *)allocate_memory(sizeof(wmelist), MISCELLANEOUS_MEM_USAGE);
    wl->nodes = (wmetree **)allocate_memory(sizeof(wmetree *) * wmelist_init_size,
                                            MISCELLANEOUS_MEM_USAGE);
    wl->cap_nodes = wmelist_init_size;
    wl->num_nodes = 0;
    wl->next = NULL;
    
    return wl;
}//make_wmelist

/* ===================================================================
   destroy_wmelist

   Deallocates a given wmelist.
   
   Created: 19 Jan 2004
   =================================================================== */
void destroy_wmelist(wmelist *wl)
{
    if (wl == NULL) return;
    
    if (wl->num_nodes > 0)
    {
        free_memory(wl->nodes, MISCELLANEOUS_MEM_USAGE);
    }
    free_memory(wl, MISCELLANEOUS_MEM_USAGE);
}//destroy_wmelist


/* ===================================================================
   symbols_are_equal_value

   This compares two symbols.  If they have the same type and value
   it returns TRUE, otherwise FALSE.  Identifiers and variables
   yield a response of TRUE.

   Created: 12 Dec 2002
   =================================================================== */
int symbols_are_equal_value(Symbol *a, Symbol *b)
{
    if (a->common.symbol_type != b->common.symbol_type)
    {
        return FALSE;
    }

    switch(a->common.symbol_type)
    {
        case SYM_CONSTANT_SYMBOL_TYPE:
            return ! (strcmp(a->sc.name, b->sc.name));
        case INT_CONSTANT_SYMBOL_TYPE:
            return a->ic.value == b->ic.value;
        case FLOAT_CONSTANT_SYMBOL_TYPE:
            return a->fc.value == b->fc.value;
        case VARIABLE_SYMBOL_TYPE:
        case IDENTIFIER_SYMBOL_TYPE:
            return TRUE;
        default:
            return FALSE;
    }

}//symbols_are_equal_value

/* ===================================================================
   wmes_are_equal_value

   This compares two wmes.  If they have the same type and value
   it returns true.

   Created: 12 Dec 2002
   =================================================================== */
int wmes_are_equal_value(wme *a, wme *b)
{
    return (symbols_are_equal_value(a->attr, b->attr))
        && (symbols_are_equal_value(a->value, b->value));

}//wmes_are_equal_value

/* ===================================================================
   wme_has_value

   This routine returns TRUE is the given WMEs attribute and value are
   both symbols and have then names given.  If either of the given
   names are NULL then they are assumed to be a match (i.e., a
   wildcard).

   Created: 14 Dec 2002
   =================================================================== */
int wme_has_value(wme *w, char *attr_name, char *value_name)
{
    if (w == NULL) return FALSE;
    
    if (attr_name != NULL)
    {
        if (w->attr->common.symbol_type != SYM_CONSTANT_SYMBOL_TYPE)
        {
            return FALSE;
        }

        if (strcmp(w->attr->sc.name, attr_name) != 0)
        {
            return FALSE;
        }
    }

    if (value_name != NULL)
    {
        if (w->value->common.symbol_type != SYM_CONSTANT_SYMBOL_TYPE)
        {
            return FALSE;
        }

        if (strcmp(w->value->sc.name, value_name) != 0)
        {
            return FALSE;
        }
    }

    return TRUE;

}//wme_has_value

/* ===================================================================
   wme_equals_node

   This routine returns TRUE is the given WME's attribute and value are
   both symbols and match the values in the given wmetree node.  If
   the value in the wmetree node is of type identifier/variable
   then it always matches.

   Created: 12 Jan 2004
   =================================================================== */
int wme_equals_node(wme *w, wmetree *node)
{
    if (w == NULL) return FALSE;

    if (w->attr->common.symbol_type != SYM_CONSTANT_SYMBOL_TYPE)
    {
        return FALSE;
    }

    //Compare attribute
    if (strcmp(w->attr->sc.name, node->attr) != 0)
    {
        return FALSE;
    }

    //Compare value
    switch(node->val_type)
    {
        case SYM_CONSTANT_SYMBOL_TYPE:
            return strcmp(w->value->sc.name, node->val.strval) == 0;

        case INT_CONSTANT_SYMBOL_TYPE:
            return w->value->ic.value == node->val.intval;

        case FLOAT_CONSTANT_SYMBOL_TYPE:
            return w->value->fc.value == node->val.floatval;
            break;

        default:
            return TRUE;
    }

}//wme_equals_node

/* ===================================================================
   print_wmetree

   
   Created: 09 Nov 2002
   =================================================================== */
void print_wmetree(wmetree *node, int indent, bool recurse)
{
    int i;
    
    if (node == NULL) return;

    if (node->parent == NULL) // check for root
    {
        print("\n\nROOT\n");
    }
    else
    {
        if (indent)
        {
            print("%*s+---", indent, "");
        }
        print("%s",node->attr);
        switch(node->val_type)
        {
            case SYM_CONSTANT_SYMBOL_TYPE:
                print(" %s", node->val.strval);
                break;
            case INT_CONSTANT_SYMBOL_TYPE:
                print(" %ld", node->val.intval);
                break;
            case FLOAT_CONSTANT_SYMBOL_TYPE:
                print(" %f", node->val.floatval);
                break;
            default:
                break;
        }//switch
        print("\n");
    }//else

    if (recurse)
    {
        for(i = 0; i < node->num_children; i++)
        {
            print_wmetree(node->children[i], indent + 4, TRUE);
        }
    }
}//print_wmetree


/* ===================================================================
   find_child_node

   Given a wmetree node and a wme, this function returns the child
   node that represents that WME (or NULL).
   
   Created: 12 Jan 2004
   =================================================================== */
wmetree *find_child_node(wmetree *node, wme *w)
{
    int i;
    
    for(i = 0; i < node->num_children; i++)
    {
        if (wme_equals_node(w, node->children[i]))
        {
            return node->children[i];
        }
    }

    return NULL;
}//find_child_node

/* ===================================================================
   add_child_to_wmetree

   This function adds a child node to the children list of another node.  
   
   Created: 12 Jan 2004
   =================================================================== */
void add_child_to_wmetree(wmetree *node, wmetree *childnode)
{
    if (node->num_children == node->cap_children)
    {
        int i;
        wmetree **tmp;
        int newsize;
        
        //Select the new array size
        newsize = childlist_init_size;
        if (node->cap_children > 0)
        {
            newsize = node->cap_children * 2;
        }
        
        //Grow the array
        tmp = (wmetree **)allocate_memory_and_zerofill(newsize * sizeof(wmetree *),
                                                       MISCELLANEOUS_MEM_USAGE);
        for(i = 0; i < node->num_children; i++)
        {
            tmp[i] = node->children[i];
        }

        if (node->children != NULL)
        {
            free_memory(node->children, MISCELLANEOUS_MEM_USAGE);
        }
        
        node->children = tmp;
        node->cap_children = newsize;
    }//if


    //Add the child to the array
    node->children[node->num_children] = childnode;
    node->num_children++;
    
    //Set the parent pointer
    childnode->parent = node;

}//add_child_to_wmetree

/* ===================================================================
   add_node_to_wmelist

   This function adds a wmetree node to a given wmelist.
   
   Created: 12 Jan 2004
   =================================================================== */
void add_node_to_wmelist(wmelist *wl, wmetree *node)
{
    if (wl->num_nodes == wl->cap_nodes)
    {
        int i;
        wmetree **tmp;
        int newsize;
        
        //Select the new array size
        newsize = wmelist_init_size;
        if (wl->cap_nodes > 0)
        {
            newsize = wl->cap_nodes * 2;
        }
        
        //Grow the array
        tmp = (wmetree **)allocate_memory_and_zerofill(newsize * sizeof(wmetree *),
                                                       MISCELLANEOUS_MEM_USAGE);
        for(i = 0; i < wl->num_nodes; i++)
        {
            tmp[i] = wl->nodes[i];
        }

        if (wl->nodes != NULL)
        {
            free_memory(wl->nodes, MISCELLANEOUS_MEM_USAGE);
        }

        wl->nodes = tmp;
        wl->cap_nodes = newsize;
    }//if


    //Add the node to the array
    wl->nodes[wl->num_nodes] = node;
    wl->num_nodes++;
    
}//add_node_to_wmelist


/* ===================================================================
   update_wmetree          *RECURSIVE*

   Updates the wmetree given a pointer to a corresponding wme in working
   memory.  The wmetree node is assumed to have been initialized already.
   Each wme that is discovered by this algorithm is also added to a given
   wmelist.

   If this function finds a ^superstate WME it does not traverse that link.
   Instead, it records the find and returns it to the caller.
   
   This function requires a tc number.

   Created: 09 Jan 2004
   =================================================================== */
Symbol *update_wmetree(wmetree *node, Symbol *sym, wmelist *wl, tc_number tc)
{
    wme **wmes;
    wmetree *childnode;
    int len = 0;
    int i;
    Symbol *ss = NULL;
    wmelist *childnodes = make_wmelist();

    wmes = get_augs_of_id( sym, tc, &len );
    if (wmes == NULL) return NULL;

    for(i = 0; i < len; i++)
    {
        childnode = find_child_node(node, wmes[i]);
        if (childnode == NULL)
        {
            childnode = make_wmetree_node(wmes[i]);
            add_child_to_wmetree(node, childnode);
        }

        //Save the childnode away for recursive update below
        add_node_to_wmelist(childnodes, childnode);

        //Check for special case: "superstate" 
        if (wme_has_value(wmes[i], "superstate", NULL))
        {
            if ( (ss == NULL)
                 && (wmes[i]->value->common.symbol_type == IDENTIFIER_SYMBOL_TYPE) )
            {
                ss = wmes[i]->value;
            }
            continue;
        }
        
        //insert childnode into the wmelist
        add_node_to_wmelist(wl, childnode);
    }//for


    //Now recursively call this function for all child nodes that
    //aren't leaf nodes.  We can't do this inside the above for-loop 
    //without messing up the transitive closure.
    for(i = 0; i < len; i++)
    {
        childnode = childnodes->nodes[i];

        //If the child isn't a leaf node, then recursively fill
        //in the child's children
        if (childnode->val_type == IDENTIFIER_SYMBOL_TYPE)
        {
            update_wmetree(childnode, wmes[i]->value, wl, tc);
        }
    }
    
    return ss;
    
}//update_wmetree

/* ===================================================================
   add_wmelist_to_memories

   This function adds a wmelist to the global memories array
   
   Created: 19 Jan 2004
   =================================================================== */
void add_wmelist_to_memories(wmelist *wl)
{
    if (g_num_memories == g_cap_memories)
    {
        int i;
        wmelist **tmp;
        int newsize;
        
        //Select the new array size
        newsize = g_cap_memories * 2;

        //Grow the array
        tmp = (wmelist **)allocate_memory_and_zerofill(newsize * sizeof(wmetree *),
                                                       MISCELLANEOUS_MEM_USAGE);
        for(i = 0; i < g_num_memories; i++)
        {
            tmp[i] = g_memories[i];
        }

        free_memory(g_memories, MISCELLANEOUS_MEM_USAGE);

        g_memories = tmp;
        g_cap_memories = newsize;
    }//if


    //Add the node to the array
    g_memories[g_num_memories] = wl;
    g_num_memories++;
    
}//add_wmelist_to_memories

/* ===================================================================
   compare_ptr

   Compares two void * pointers.  (Used for qsort() calls)
   
   Created: 06 Nov 2002
   =================================================================== */
int compare_ptr( const void *arg1, const void *arg2 )
{
    return *((long *)arg1) - *((long *)arg2);
}

/* ===================================================================
   record_epmem

   Once it has been determined that an epmem needs to be recorded,
   this routine manages all the steps for recording it.

   Created: 12 Jan 2004
   =================================================================== */
void record_epmem( )
{
    tc_number tc;
    Symbol *sym;
    wmelist *curr_state;
    wmelist *next_state;

    tc = current_agent(bottom_goal)->id.tc_num++;

    //Starting with bottom_goal and moving toward top_goal, add all
    //the current states to the wmetree and record the full WM
    //state as a list of wmelist structs
    sym = current_agent(bottom_goal);
    
    //%%%Do only top-state for now
    sym = current_agent(top_goal);  //%%%remove this later
    
    curr_state = NULL;
    next_state = NULL;
    while(sym != NULL)
    {
        next_state = make_wmelist();
        next_state->next = curr_state;
        curr_state = next_state;

        sym = update_wmetree(&g_wmetree, sym, curr_state, tc);
    }

    //Sort the wmelists
    next_state = curr_state;
    while(next_state != NULL)
    {
        qsort( (void *)next_state->nodes,
               (size_t)next_state->num_nodes,
               sizeof( wmetree * ),
               compare_ptr );
        next_state = next_state->next;
    }

    //Store the recorded memory
    add_wmelist_to_memories(curr_state);
    

    //%%%UNCOMMENT TO DEBUG:
    //print_wmetree(&(g_wmetree), 0, TRUE);


    
}//record_epmem


/* ===================================================================
   wme_array_diff()

   Given two arrays of wme* this routine determines how many are different.
   Both arrays must be sorted order.
   
   Created: 08 Nov 2002
   =================================================================== */
int wme_array_diff(wme **arr1, wme** arr2, int len)
{
    int count = len;
    int pos1 = 0;
    int pos2 = 0;

    while((pos1 < len) && (pos2 < len))
    {
        if (arr1[pos1] == arr2[pos2])
        {
            count--;
            pos1++;
            pos2++;
        }
        else if (arr1[pos1] < arr2[pos2])
        {
            pos1++;
        }
        else
        {
            pos2++;
        }
    }//while

    return count;

}//wme_array_diff

/* ===================================================================
   wme_to_skip

   This is a helper function for get_most_activated_wmes.  Certain
   WMEs from the environment update every cycle (e.g., ^random) and
   thus gain high activation every cycle.  However, since the value is
   different and virtually unique every time they cause recall
   proposal rules to be created that will never fire.  To prevent
   this, WMEs with certain hard-coded names are ignored when
   retrieving the top n most activated wmes.  I realize this isn't
   a very psychologically plausible fix for this problem but it
   will do in the near term.

   Currently Ignoring these Tanksoar WMEs:
     cycle
     random
     clock
     x
     y
   And these Eaters WMEs:
     ^name eaters
     cycle
     score
     eater-score
     x
     y
   And these WMEs which are part of the performance data recorded by
   the eaters agent and not part of the agent proper.
     prev-score
     reward
     score
     x
     y
     move-count
     could-reflect
    

   %%%TODO:  Remove the need for this function.
     
   Created: 12 Dec 2002
   =================================================================== */
int wme_to_skip(wme *w)
{
    char *s;
    
    if (w->attr->common.symbol_type != SYM_CONSTANT_SYMBOL_TYPE)
    {
        return FALSE;
    }

    s = w->attr->sc.name;
    switch(s[0])
    {
        case 'c':
            switch(s[1])
            {
                case 'y': return strcmp(s, "cycle") == 0;
                case 'l': return strcmp(s, "clock") == 0;
                case 'o': return strcmp(s, "could-reflect") == 0;
                default:  return FALSE;
            }
        case 'e':
            return strcmp(s, "eater-score") == 0;
        case 'm':
            return strcmp(s, "move-count") == 0;
        case 'n':
            if (    (strcmp(s, "name") == 0)
                 && (strcmp(w->value->sc.name, "eaters") == 0) )
            {
                return TRUE;
            }
            return FALSE;
        case 'p':
            return strcmp(s, "prev-score") == 0;
        case 's':
            return strcmp(s, "score") == 0;
        case 'r':
            if (s[1] == 'a') return strcmp(s, "random") == 0;
            if (s[1] == 'e') return strcmp(s, "reward") == 0;
            return FALSE;
        case 'x':
            return s[1] == '\0';
        case 'y':
            return s[1] == '\0';
        default:
            return FALSE;
    }//switch
}//wme_to_skip

/* ===================================================================
   get_most_activated_wmes

   Use the decay wmes array to retrive the n most activated wmes.  The
   given array should already be allocated and of length n. This
   routine can can be directed to favor leaf wmes (i.e., a wme whose
   value is not an identifier) via the threshold parameter.  The
   threshold specifies how much more activated a non-leaf wme must be
   than a leaf wme for it to be acceptable.  This threshold is
   specified as an nonnegative integer indicating how many positions
   the leaf wme must be behind the non-leaf wme in the decay queue
   (i.e., current_agent(decay_timelist)).  A value of 0 for this
   threshold indicates no preference between non-leaf and leaf wmes.
   The  value MAX_DECAY assures that leaf WMEs will always
   be selected.

   This function returns the actual number of wmes that were placed in
   given array.  
   
   Created: 06 Nov 2002
   Changed: 25 April 2003 - added leaf preference threshold
   =================================================================== */

int get_most_activated_wmes(wme **active_wmes, int n, int nonleaf_threshold)
{
    decay_timelist_element *decay_list;
    int pos = 0;
    int decay_pos;              // current position in the decay array
    int nl_decay_pos = -1;      // non-leaf decay pos
    wme_decay_element *decay_element;
    int i;

    decay_list = current_agent(decay_timelist);
    decay_pos = current_agent(current_decay_timelist_element)->position;

    for(i = 0; i < MAX_DECAY; i++)
    {
        //Traverse the decay array backwards in order to get the most
        //activated wmes first
        decay_pos = decay_pos > 0 ? decay_pos - 1 : MAX_DECAY - 1;
        
        /*
         * Search for leaf wmes first
         */
        if (decay_list[decay_pos].first_decay_element != NULL)
        {
            decay_element = decay_list[decay_pos].first_decay_element;
            while (decay_element != NULL)
            {
                if ( (IS_LEAF_WME(decay_element->this_wme))
                     && (! wme_to_skip(decay_element->this_wme)) )
                {
                    active_wmes[pos] = decay_element->this_wme;
                    pos++;
                }
                decay_element = decay_element->next;
                if (pos == n) return pos;
            }
        }//if
        
        /*
         * Search for non-leaf wmes that are above the threshold
         */
        if (i >= nonleaf_threshold)
        {
            nl_decay_pos = decay_pos - nonleaf_threshold;
            if (nl_decay_pos < 0)
            {
                nl_decay_pos = nl_decay_pos + MAX_DECAY;
            }
        
            if (decay_list[nl_decay_pos].first_decay_element != NULL)
            {
                decay_element = decay_list[nl_decay_pos].first_decay_element;
                while (decay_element != NULL)
                {
                    if (! IS_LEAF_WME(decay_element->this_wme) )
                    {
                        active_wmes[pos] = decay_element->this_wme;
                        pos++;
                    }
                    
                    decay_element = decay_element->next;
                    if (pos == n) return pos;
                }
            }//if
        }//if
        
    }//for
    
    
    /*
     * In the rare case there aren't enough WMEs to fill the array
     * set the remaining entries to NULL.
     */
    if (pos < n)
    {
        //%%%print("\nNot Enough WMEs in working memory!\n");
        for(i = pos; i < n; i++)
        {
            active_wmes[i] = NULL;
        }
    }//if

    return pos;

}//get_most_activated_wmes


/* ===================================================================
   consider_new_epmem() 

   This routine decides whether the current state is worthy of being
   remembered as an episodic memory.  If so, record_epmem() is called.

   Created: 06 Nov 2002
   =================================================================== */

void consider_new_epmem()
{
    int i;

    i = get_most_activated_wmes(g_current_active_wmes, num_active_wmes, MAX_DECAY);
    if (i < num_active_wmes)
    {
        // no WMEs are activated right now
        return; 
    }

    //See if enough of these WMEs have changed
    qsort( (void *)g_current_active_wmes, (size_t)num_active_wmes, sizeof( wme * ),compare_ptr );
    i = wme_array_diff(g_current_active_wmes, g_previous_active_wmes, num_active_wmes);
    
    if ( i >= num_wmes_changed )
    {
        //Save the WMEs used for this memory in order to compare for
        //the next memory
        for(i = 0; i < num_active_wmes; i++)
        {
            g_previous_active_wmes[i] = g_current_active_wmes[i];
        }
        
        record_epmem();         // The big one

    }
    //%%%UNCOMMENT TO DEBUG
    else
    {
        //print("\nEpmem not recorded due to lack of change in WM.\n");
    }

    
}//consider_new_epmem

/* ===================================================================
   epmem_clear_mem               *RECURSIVE*

   This routine removes all the epmem WMEs from working memory that
   are associated with a given wmetree (via ->assoc_wme).

   Created: 16 Feb 2004
   =================================================================== */
void epmem_clear_mem(wmetree *node)
{
    int i;

    //Find all the wmes attached to the given node
    for(i = 0; i < node->num_children; i++)
    {
        if (! node->children[i]->in_mem) continue;
        if (strcmp(node->children[i]->attr, "epmem") == 0) continue;
        if (node->children[i]->assoc_wme == NULL)
        {
            print("\nERROR: WME should be in memory for: ");
            print_wmetree(node->children[i], 0, FALSE);
            continue;
        }

        //Recursive call.  Do this first so leaf WMEs are
        //deallocated first.
        epmem_clear_mem(node->children[i]);

        //%%%REMOVE THIS
//          print("\t\t\tREMOVING:  ");
//          print_wme(node->children[i]->assoc_wme);

        //Remove from WM
        remove_input_wme(node->children[i]->assoc_wme);
        wme_remove_ref(node->children[i]->assoc_wme);

        //Bookkeeping
        node->children[i]->assoc_wme = NULL;
        node->children[i]->in_mem = FALSE;
    }//for
}//epmem_clear_mem


/* ===================================================================
   compare_wmelist

   Compares two wmelists and returns the number of WMEs they have
   in common.  Obviously both lists should reference the same wmetree
   for this comparison to be useful.
   
   Created: 19 Jan 2004
   =================================================================== */
int compare_wmelist(wmelist *w1, wmelist *w2)
{
    int count = 0;
    int pos1 = 0;
    int pos2 = 0;

    while((pos1 < w1->num_nodes) && (pos2 < w2->num_nodes))
    {
        if (w1->nodes[pos1] == w2->nodes[pos2])
        {
            count++;
            pos1++;
            pos2++;
        }
        else if (w1->nodes[pos1] < w2->nodes[pos2])
        {
            pos1++;
        }
        else
        {
            pos2++;
        }
    }//while

    return count;
}//compare_wmelist

/* ===================================================================
   match_wmelist

   Finds the wmelist in g_memories that most closely matches the
   one given.  If no match is found, a NULL value is returned.
   If the next boolean is set, the memory returned is the one
   that occurs *after* the closest match (in temporal order).

   Created:  19 Jan 2004
   =================================================================== */
wmelist *match_wmelist(wmelist *w, bool next)
{
    int best_match = 0;
    int best_index = 0;
    int n;
    int i;

    for(i = 0; i < g_num_memories; i++)
    {
        n = compare_wmelist(g_memories[i], w);
        if (n >= best_match)
        {
            best_index = i;
            best_match = n;
        }
    }

    if ( (next) && (best_index + 1 < g_num_memories) )
    {
        return g_memories[best_index + 1];
    }
    
    return g_memories[best_index];
}//match_wmelist

/* ===================================================================
   iwiw_helper                *RECURSIVE*

   helper function for install_wmelist_in_wm.

   Created: 20 Jan 2004
   =================================================================== */
void iwiw_helper(Symbol *id, wmetree *node)
{
    int i;
    Symbol *attr;
    Symbol *val;

    //Create the wmes attached to sym
    for(i = 0; i < node->num_children; i++)
    {
        if (! node->children[i]->in_mem) continue;
        //For now, avoid recursing into previous memories.
        if (strcmp(node->children[i]->attr, "epmem") == 0) continue;
        if (node->children[i]->assoc_wme != NULL)
        {
            //%%%I think this is an error.  What do I do about it?
        }
        
        attr = make_sym_constant(node->children[i]->attr);

        switch(node->children[i]->val_type)
        {
            case SYM_CONSTANT_SYMBOL_TYPE:
                val = make_sym_constant(node->children[i]->val.strval);
                break;
            case INT_CONSTANT_SYMBOL_TYPE:
                val = make_int_constant(node->children[i]->val.intval);
                break;
            case FLOAT_CONSTANT_SYMBOL_TYPE:
                val = make_float_constant(node->children[i]->val.floatval);
                break;

            default:
                val = make_new_identifier(node->children[i]->attr[0],
                                          id->id.level);
                break;
        }//switch

        node->children[i]->assoc_wme = add_input_wme(id, attr, val);
        wme_add_ref(node->children[i]->assoc_wme);

        //%%%REMOVE THIS
//      print("\t\t\tADDING:  ");
//      print_wme(node->children[i]->assoc_wme);

        
        //Recursively create wmes for children's children
        iwiw_helper(val, node->children[i]);

    }//for
}//iwiw_helper

/* ===================================================================
   install_wmelist_in_wm

   Given a wmelist and a symbol, this function recreates the working
   memory fragment represented by the wmelist on the symbol.

   Created:  19 Jan 2004
   =================================================================== */
void install_wmelist_in_wm(Symbol *sym, wmelist *wl)
{
    int i;

    //set the in_mem flag on all wmes in the wmelist
    for(i = 0; i < wl->num_nodes; i++)
    {
        wl->nodes[i]->in_mem = TRUE;
    }

    //Use the recursive helper function to create the wmes
    iwiw_helper(sym, &(g_wmetree));
    
}//install_wmelist_in_wm

/* ===================================================================
   respond_to_query()

   This routine examines a query attached to a given symbold.  The result
   is attached to another given symbol.  Any existing WMEs in the retrieval
   buffer are removed.  The caller may optionally request that the
   next memory (in temporal sequence) from the best match be retrieved
   instead of the best match itself.

   The routine returns a pointer to the wmelist representing the
   memory which was placed on the the retrieved link (or NULL).

   query - the query
   retrieved - where to install the retrieved result
   next - whether to install the best fit or the next memory

   Created: 19 Jan 2004
   =================================================================== */
wmelist *respond_to_query(Symbol *query, Symbol *retrieved, bool next)
{
    wmelist *wl_query;
    wmelist *wl_retrieved;
    tc_number tc;
    wme *w;

    //Create a wmelist representing the current query
    wl_query = make_wmelist();
    tc = current_agent(bottom_goal)->id.tc_num++;
    update_wmetree(&g_wmetree, query, wl_query, tc);

    //Remove the old retrieved memory
    epmem_clear_mem(&g_wmetree);

    //If the query is empty then we're done
    if (wl_query->num_nodes == 0) return NULL;

    //Match query to current memories list
    wl_retrieved = match_wmelist(wl_query, next);

    //Place the best fit on the retrieved link
    if (wl_retrieved != NULL)
    {
        install_wmelist_in_wm(retrieved, wl_retrieved);
    }
    else
    {
        //Notify the user of failed retrieval
        w = add_input_wme(retrieved,
                          make_sym_constant("no-retrieval"),
                          make_sym_constant("true"));
        wme_add_ref(w);
    }

    return wl_retrieved;

}//respond_to_query

/* ===================================================================
   epmem_retrieve_command() 

   This routine examines the ^epmem link for a command.  Commands are
   always of the form (<s> ^epmem.command cmd) where "cmd" is a
   symbolic constant.  If a command is found its value is returned to
   the caller.  Otherwise NULL is returned.  The pointer returned is a
   direct reference to the WME so the pointer may *not* be valid on
   subsequent cycles.
   
   Created: 27 Jan 2004
   =================================================================== */
char *epmem_retrieve_command()
{
    wme **wmes;
    int len = 0;
    int i;
    tc_number tc;
    char *ret = NULL;
    
    tc = current_agent(bottom_goal)->id.tc_num++;
    wmes = get_augs_of_id( g_epmem_query, tc, &len );
    if (wmes == NULL) return NULL;

    for(i = 0; i < len; i++)
    {
        if ( (wme_has_value(wmes[i], "command", NULL))
             && (wmes[i]->value->common.symbol_type == SYM_CONSTANT_SYMBOL_TYPE) )
        {
            ret = wmes[i]->value->sc.name;
            remove_wme_from_wm(wmes[i]);
        }
    }

    return ret;
}//epmem_retrieve_command

/* ===================================================================
   increment_retrieval_count

   Increments the value stored in ^epmem.retieval-count by a positive
   integer. If the user passes a zero value then the present value is
   reset to 1.  If the user passes a negative value then the present
   value is removed from working memory.

   27 Jan 2004
   =================================================================== */
void increment_retrieval_count(long inc_amt)
{
    wme **wmes;
    int len = 0;
    int i;
    tc_number tc;
    long current_count = 0;
    wme *w;

    //Find the (epmem ^retreival-count n) WME, save the value,
    //and remove the WME from WM
    tc = current_agent(bottom_goal)->id.tc_num++;
    wmes = get_augs_of_id( g_epmem_header, tc, &len );
    if (wmes == NULL) return;
    for(i = 0; i < len; i++)
    {
        if ( (wme_has_value(wmes[i], "retrieval-count", NULL))
             && (wmes[i]->value->common.symbol_type == INT_CONSTANT_SYMBOL_TYPE) )
        {
            current_count = wmes[i]->value->ic.value;
            remove_input_wme(wmes[i]);
            break;
        }
    }

    //Check for remove only
    if (inc_amt < 0)
    {
        return;
    }

    //Calculate the new retrieval count
    current_count += inc_amt;
    if (inc_amt == 0)
    {
        current_count = 1;
    }

    //Install a new WME
    w = add_input_wme(g_epmem_header,
                      make_sym_constant("retrieval-count"),
                      make_int_constant(current_count));
    wme_add_ref(w);


}//increment_retrieval_count

/* ===================================================================
   respond_to_command() 

   This routine responds to agent commands given on the ^epmem link.
   The following commands are supported:
       "next" - populate ^epmem.retrieved with the next memory
                in the sequence

   Created: 27 Jan 2004
   =================================================================== */
void respond_to_command(char *cmd)
{
    wmelist *wl;
    
    if (strcmp(cmd, "next") == 0)
    {
        wl = respond_to_query(g_epmem_retrieved, g_epmem_retrieved, TRUE);
        if (wl != NULL)
        {
            increment_retrieval_count(1);
        }
    }
    
}//respond_to_command



/* ===================================================================
   epmem_update() 

   This routine is called at every output phase to allow the episodic
   memory system to update its current memory store and respond to any
   queries. 

   Created: 19 Jan 2004
   =================================================================== */
void epmem_update()
{
    char *cmd;
    wmelist *wl = NULL;

    //Consider recording a new epmem
    consider_new_epmem();

    //Update the ^retrieved link as necessary
    cmd = epmem_retrieve_command();
    if (cmd != NULL)
    {
        respond_to_command(cmd);
    }
    else
    {
        wl = respond_to_query(g_epmem_query, g_epmem_retrieved, FALSE);
        if (wl != NULL)
        {
            //New retrieval:  reset count to zero
            increment_retrieval_count(0);
        }
        else
        {
            //No retrieval:  remove count from working memory
            increment_retrieval_count(-1);
        }
    }

}//epmem_update

/* ===================================================================
   epmem_create_buffer()

   This routine creates the ^epmem link on a given state.  This is
   used to query the episodic memory and provided the retrieved
   result.

   Created: 08 Jan 2004
   =================================================================== */

void epmem_create_buffer(Symbol *s)
{
    wme *w;
    
    //Add the ^epmem header WMEs
    g_epmem_header = make_new_identifier('E', s->id.level);
    g_epmem_query = make_new_identifier('E', s->id.level);
    g_epmem_retrieved = make_new_identifier('E', s->id.level);

    w = add_input_wme(s, make_sym_constant("epmem"), g_epmem_header);
    wme_add_ref(w);
    w = add_input_wme(g_epmem_header, make_sym_constant("query"), g_epmem_query);
    wme_add_ref(w);
    w = add_input_wme(g_epmem_header, make_sym_constant("retrieved"), g_epmem_retrieved);
    wme_add_ref(w);

    
}//epmem_create_buffer


/* ===================================================================
   init_epemem()

   This routine is called once to initialize the episodic memory system.

   Created: 03 Nov 2002
   =================================================================== */

void init_epmem(void) 
{
    //Allocate the active wmes arrays
    g_current_active_wmes = (wme **)calloc(num_active_wmes, sizeof(wme *));
    g_previous_active_wmes = (wme **)calloc(num_active_wmes, sizeof(wme *));

    //Initialize the wmetree
    g_wmetree.attr = NULL;
    g_wmetree.val.intval = 0;
    g_wmetree.val_type = IDENTIFIER_SYMBOL_TYPE;
    g_wmetree.children = NULL;
    g_wmetree.cap_children = 0;
    g_wmetree.num_children = 0;
    g_wmetree.parent = NULL;

    //Initialize the memories array
    g_memories = (wmelist **)allocate_memory_and_zerofill(memories_init_size * sizeof(wmelist *),
                                                          MISCELLANEOUS_MEM_USAGE);
    g_cap_memories = memories_init_size;
    g_num_memories = 0;

    
}/*init_epmem*/




#endif /* AMN end */

/* ===================================================================
   =================================================================== */
