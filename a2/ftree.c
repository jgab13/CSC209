#include <stdio.h>
// Add your system includes here.
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include "ftree.h"


struct TreeNode *construct_node(const char * fname, char * path){
    
    int size = strlen(path) + strlen(fname) + 2;
    char new_path[size];
    strncpy(new_path, path, sizeof(new_path));
    strncat(new_path, fname, sizeof(new_path) - strlen(new_path) - 1);
    //printf("%s \n", new_path);

    struct stat stat_buf;
    
    if (lstat(new_path, &stat_buf) == -1) {
        fprintf(stderr, "The path (%s) does not point to an existing entry!\n", fname);
        return NULL;
    }
    
    //case 2: File exists (link, regular file or directory with nothing else, 
    //construct root node using malloc. store values in the node.
    struct TreeNode * tree;
    tree = malloc(sizeof(struct TreeNode));
    tree->fname = malloc(sizeof(char)*(strlen(fname)+1));
    if (tree == NULL){
    	printf("The malloc call did not work.");
	return NULL;
    }
    
    //copy name and permissions into tree
    strcpy(tree->fname, fname);
    //printf("The tree fname is %s \n", tree->fname);
    tree->permissions = stat_buf.st_mode & 0777;
    //printf("the %s permissions are %o\n", tree->fname, tree->permissions);
    
    //determine file type and set in tree struct.
    if (S_ISREG(stat_buf.st_mode)) {
    	tree->type = '-';
    } else if (S_ISDIR(stat_buf.st_mode)){
    	tree->type = 'd';
    } else if (S_ISLNK(stat_buf.st_mode)){
    	tree->type = 'l';
    }
    tree->next = NULL;
    tree->contents = NULL;
    return tree;
}


struct TreeNode *construct_tree(const char * path){
    
    DIR *d_ptr = opendir(path);
    //error checking to see if file opened correctly.
    if (d_ptr == NULL) {
        perror("Direent did not open correctly. ");
        return NULL;
    }
    
	struct TreeNode *root = NULL;
	struct TreeNode *current = NULL;
	struct TreeNode *nex = NULL;
	
	struct dirent *entry_ptr;
    entry_ptr = readdir(d_ptr);

	while (entry_ptr != NULL) {
		if ((strcmp(entry_ptr->d_name, ".")) && (strcmp(entry_ptr->d_name, ".."))){ //May need this for error check to skip . and ..
				//printf("Name: %s permissions: %o type: %c\n", current->fname, 
				//current->permissions, current->type);
			int size = strlen(path) + 2;
			char new_path[size];
			strncpy(new_path, path, sizeof(new_path));
			strncat(new_path, "/", sizeof(new_path) - strlen(new_path) - 1);

			nex = construct_node(entry_ptr->d_name, new_path);
			
			if (root == NULL){
				root = nex;
				current = nex;
			} else {
				current->next = nex;
				current = current->next;
			}
			
			if (nex->type == 'd'){

				int size2 = strlen(new_path) + strlen(entry_ptr->d_name) + 2;
    			char new_path2[size2];
    			strncpy(new_path2, new_path, sizeof(new_path2));
    			strncat(new_path2, entry_ptr->d_name, sizeof(new_path2) - strlen(new_path2) - 1);

				current->contents = construct_tree(new_path2);
			}
			nex = nex->next;
			


			
			
		}
		entry_ptr = readdir(d_ptr);	
	}
    
    if (closedir(d_ptr) == -1){
		perror("Closed d_ptr failed");
		return root;
    }
  
    return root;
}





/*
 * Returns the FTree rooted at the path fname.
 *
 * Use the following if the file fname doesn't exist and return NULL:
 * fprintf(stderr, "The path (%s) does not point to an existing entry!\n", fname);
 *
 */
struct TreeNode *generate_ftree(const char *fname) {

    // Your implementation here.

    // Hint: consider implementing a recursive helper function that
    // takes fname and a path.  For the initial call on the 
    // helper function, the path would be "", since fname is the root
    // of the FTree.  For files at other depths, the path would be the
    // file path from the root to that file.
    
	struct TreeNode * root = construct_node(fname, "");
	if ((root != NULL) && (root->type == 'd')){
		root->contents = construct_tree(fname);
    }
	return root;
	
	
}


/*
 * Prints the TreeNodes encountered on a preorder traversal of an FTree.
 *
 * The only print statements that you may use in this function are:
 * printf("===== %s (%c%o) =====\n", root->fname, root->type, root->permissions)
 * printf("%s (%c%o)\n", root->fname, root->type, root->permissions)
 *
 */
void print_ftree(struct TreeNode *root) {
	
    // Here's a trick for remembering what depth (in the tree) you're at
    // and printing 2 * that many spaces at the beginning of the line.
    //case 1: only for a directory with nothing else in it.
    static int depth = 0;
	if (root != NULL){
		printf("%*s", depth * 2, "");
		if (root->type == 'd'){
			printf("===== %s (%c%o) =====\n", root->fname, root->type, root->permissions);
			if (root->contents != NULL){			
				depth += 1;
				print_ftree(root->contents);
				depth -= 1;
			}	
			if (root->next != NULL){
				print_ftree(root->next);
			}
		} else {
			printf("%s (%c%o)\n", root->fname, root->type, root->permissions);
			if (root->next != NULL){
				print_ftree(root->next);
			}
		}
    }
}


/* 
 * Deallocate all dynamically-allocated memory in the FTree rooted at node.
 * 
 */
void deallocate_ftree (struct TreeNode *node) {
	if (node != NULL){
		if ((node->next == NULL) && (node->contents == NULL)){
			free(node->fname);
			free(node);
		} else if ((node->next != NULL) && (node->contents == NULL)){
			deallocate_ftree(node->next);
			free(node->fname);
			free(node);
		} else if ((node->contents != NULL) && (node->next == NULL)){
			deallocate_ftree(node->contents);
			free(node->fname);
			free(node);
		} else if ((node->contents != NULL) && (node->next != NULL)){
			deallocate_ftree(node->contents);
			deallocate_ftree(node->next);
			free(node->fname);
			free(node);
		}
	}
}
