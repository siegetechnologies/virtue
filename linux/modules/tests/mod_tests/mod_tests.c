#include <linux/kernel.h>
#include <linux/module.h>
#include "../../include/mod_allocator.h"


int allocator_tests(void){
	int i;
	void *t1;
	void *t2;
	printk("Running allocator_tests\n");
	//basic basic test... 16 bytes alloc
	t1 = maven_alloc(16);
	if(!t1){
		printk("Basic alloc  %s:%i failed\n", __FILE__, __LINE__);
		return 1;
	}
	maven_free(t1);
	//basic alignment test
	for(i = 0; i < 100; i++){
		t1 = maven_alloc_aligned(16, 16);
		if(!t1){
			printk("Basic aligned alloc %s:%i failed\n", __FILE__, __LINE__);
			return 1;
		}
		maven_free(t1);
		if((uintptr_t)t1 % 16){
			printk("Basic aligned alloc not aligned %s:%i failed\n", __FILE__, __LINE__);
			return 1;
		}
	}



	for(i = 0; i < 100; i++){
		//basic realloc test... should not move
		t1 = maven_realloc(0, 32);
		if(!t1){
			printk("Basic realloc null-alloc %s:%i failed\n", __FILE__, __LINE__);
			return 1;
		}
		t2 =maven_realloc(t1, 10);
		if(t2 != t1){
			printk("Basic realloc smaller_no_movement  %s:%i failed\n", __FILE__, __LINE__);
			maven_free(t2);
			return 1;
		}
		//this only works because 20 < 32
		t1 =maven_realloc(t2, 20);
		if(t2 != t1){
			printk("Basic realloc larger_no_movement  %s:%i failed\n", __FILE__, __LINE__);
			maven_free(t1);
			return 1;
		}
		t2 =maven_realloc(t1, 80);
		if(t2 == t1){
			printk("Basic realloc larger_movement %s:%i failed\n", __FILE__, __LINE__);
			maven_free(t2);
			return 1;
		}
		/*
		//todo, once we implement a smaller that puts it in a smaller block, do that test
		t1 = maven_realloc(t2, 10);
		if(t2 == t1){
			printk("Basic realloc smaller_movement  %s:%i failed\n", __FILE__, __LINE__);
			maven_free(t1);
			return 1;
		}
		*/
		//make sure to update this!
		maven_free(t2);
	}
	//realloc_aligned
	for(i = 0; i < 100; i++){
		t1 = maven_alloc(721);
		if(!t1){
			printk("realloc_aligned orig-alloc %s:%i failed\n", __FILE__, __LINE__);
			return 1;
		}
		//smaller than the origional, so it should put it in the same box
		t2 = maven_realloc_aligned(t1, 700, 16);
		if((uintptr_t)t2 % 16){
			printk("Basic aligned alloc not aligned %s:%i failed\n", __FILE__, __LINE__);
			maven_free(t2);
			return 1;
		}
		maven_free(t2);
	}

	//double_free
	for(i = 0; i < 3; i++){	//need multiple used blocks in one pool to not trigger preliminary num_free >= num_blocks protection
		t1 = maven_alloc(721);
		if(!t1){
			printk("double_free orig-alloc %s:%i failed\n", __FILE__, __LINE__);
			return 1;
		}
	}
	maven_free(t1);
	maven_free(t1);

	return 0;
}


static __init int tests_init(void){
	if(allocator_tests()){
		printk(KERN_WARNING "allocator_tests failed\n");
		return - EAGAIN;
	}
	printk(KERN_WARNING "allocator_tests completed with no errors!\n");
	printk(KERN_WARNING "all tests completed with no errors!\n");
	return 0;
}

static __exit void tests_exit(void){
	//dont really do anything...
	printk("unloading tests\n");
}


MODULE_LICENSE("GPL");
module_init(tests_init);
module_exit(tests_exit);
