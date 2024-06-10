// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.

	if ((err & FEC_WR) && (uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_COW)) {
		

		// Allocate a new page, map it at a temporary location (PFTEMP),
		// copy the data from the old page to the new page, then move the new
		// page to the old page's address.
		// Hint:
		//   You should make three system calls.

		// LAB 4: Your code here.

		if (sys_page_alloc(0, PFTEMP, PTE_W | PTE_U | PTE_P) < 0) {
			panic("error allocating page");
		}

		void *pg_start = ROUNDDOWN(addr, PGSIZE);
		memcpy(PFTEMP, pg_start, PGSIZE);

		if (sys_page_map(0, PFTEMP, 0, pg_start, PTE_W | PTE_U | PTE_P) < 0) {
			panic("error mapping page");
		}

		if (sys_page_unmap(0, PFTEMP) < 0) {
			panic("error unmapping temp page");
		}
		return;
	}
	panic("pgfault Not a write or copy-on-write page");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	void *page = (void*)(pn * PGSIZE);
	if (uvpt[pn] & PTE_W || uvpt[pn] & PTE_COW) {
		int pg;
		pg = sys_page_map(0, page, envid, page, PTE_P | PTE_U | PTE_COW);
		if (pg < 0) {
			return pg;
		}

		pg = sys_page_map(0, page, 0, page, PTE_P | PTE_U | PTE_COW);
		if (pg < 0) {
			return pg;
		}
	} else {
		int pg = sys_page_map(0, page, envid, page, PTE_U | PTE_P);
		if (pg < 0) {
			return pg;
		}
	}

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	set_pgfault_handler(pgfault);
	envid_t env_id = sys_exofork();

	if (env_id == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	if (env_id < 0) {
		return env_id;
	}
	uintptr_t addr;
	for (addr = 0; addr < USTACKTOP; addr += PGSIZE) {
		if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_U)) {
			duppage(env_id, PGNUM(addr));
		}
	}

	if (sys_page_alloc(env_id, (void *)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W) < 0) {
		panic("error on page alloc");
	}

	extern void _pgfault_upcall();
	if (sys_env_set_pgfault_upcall(env_id, _pgfault_upcall) < 0) {
		panic("error on pagefault upcall set");
	}
	
	if (sys_env_set_status(env_id, ENV_RUNNABLE) < 0) {
		panic("error on env set status");
	}
	return env_id;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}