
#include <linux/file.h>
#include "./snvme_help.h"
#include "ext4.h"

static inline ext4_lblk_t ext4_es_end(struct extent_status *es)
{
	BUG_ON(es->es_lblk + es->es_len < es->es_lblk);
	return es->es_lblk + es->es_len - 1;
}

struct extent_status *nds_do_search_extent(struct rb_root *root, __u32 lblk)
{
	struct rb_node *node = root->rb_node;
	struct extent_status *es = NULL;

	while (node) {
		es = rb_entry(node, struct extent_status, rb_node);
		if (lblk < es->es_lblk)
			node = node->rb_left;
		else if (lblk > ext4_es_end(es))
			node = node->rb_right;
		else
			return es;
	}

	if (es && lblk < es->es_lblk)
		return es;

	if (es && lblk > ext4_es_end(es)) {
		node = rb_next(&es->rb_node);
		return node ? rb_entry(node, struct extent_status, rb_node) :
			      NULL;
	}

	return NULL;
}
int nds_ext4_retrieve_mapping(struct inode *inode, loff_t offset, loff_t len, struct nds_mapping *mapping)
{
	__u64 i_lblk_start, i_lblk_end;
	struct ext4_es_tree *tree;
	struct extent_status *es1 = NULL;
	u8 blkbits = inode->i_blkbits;
	int found = 0;

	i_lblk_start = offset >> blkbits;
	i_lblk_end = (offset + len - 1) >> blkbits;

	tree = &EXT4_I(inode)->i_es_tree;
	if (tree->cache_es) {
		es1 = tree->cache_es;
		if (in_range(i_lblk_start, es1->es_lblk, es1->es_len)) {
			es_debug("%llu cached by [%u/%u)\n",
				 i_lblk_start, es1->es_lblk, es1->es_len);
			found = 1;
			goto out;
		}
	}
	
	es1 = nds_do_search_extent(&tree->root,i_lblk_start);
	if(!es1 || es1->es_lblk>i_lblk_end)
	{
		mapping->exist = false;
	}
	else{
		found = 1;
	}
		
out:
	if(found)
	{
		BUG_ON(!es1);
		// printk("The es_lblk is %lu, es_len is %lu, es_pblk is %lx",es1->es_lblk,es1->es_len,ext4_es_pblock(es1));
		loff_t in_extent_offset = offset - (((u64)es1->es_lblk) << blkbits);
		loff_t in_extent_len = min(len, (((u64)es1->es_len) << blkbits) - in_extent_offset);
		mapping->exist = true;
		mapping->offset = offset;
		mapping->allocated_len = in_extent_len;
		mapping->address = (ext4_es_pblock(es1) << blkbits) + in_extent_offset;
		mapping->blkbit = blkbits;
	}
	return found;
}

int nds_retrieve_mapping(struct nds_mapping *mapping)
{
    int found;
	struct file *rw_file = fget(mapping->file_fd);
	struct inode *rw_file_inode = file_inode(rw_file);
	
	if(rw_file_inode == NULL)
		return -EINVAL;
	found = nds_ext4_retrieve_mapping(rw_file_inode,mapping->offset,mapping->len,mapping);
	fput(rw_file);
	return found;
}