### Buddy memory
Detatils can be found in the buddy.c file.

### On detection of double free
In the page\_free function, there is a check to see
if the page -> pp\_free is already set.

### On invalid free
We check if the first physical addr of a page is actually 
aligned to PAGE\_SIZE.
We also check, if the page is not the primary block, that the
primary block does not have a bigger order.

### On UAF
We zero a page before return it to the caller.
Also, we tried to implement a way to randomly choose one from all
the possible candidates. We tried to implement the randomness function
with the BIOS time.
But we met a problem.
We didn't have enough time to investigate this.

### On out of bound of writes
When a page of order 0 is requested,
we find a page with order 2.
Like:
|-|-|-|-|
 1 2 3 4
We return the 3rd page, and we clear all the other pages(due to no randomness support).
When a page of order 0 is to be freed, we recover the guard pages and check they are actually are 0.
And free the page of order 2.
However, this breaks the original tests:(
