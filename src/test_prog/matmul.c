#include <syscall.h>
#define maxn 10

int n;
int a[maxn][maxn], b[maxn][maxn], c[maxn][maxn];

void solve()
{
  	for (int i = 0; i < n; ++i)
	    	for (int j = 0; j < n; ++j) 
	    	{
	    		a[i][j] = i*n + j + 1;
	    		b[i][j] = j*n + i + 1;
	    	}

	for (int i = 0; i < n; ++i)
		for (int j = 0; j < n; ++j)
		{
	      		c[i][j] = 0;
	      		for (int k = 0; k < n; ++k)
				c[i][j] += a[i][k] * b[k][j];
    		}
}

int main()
{
	n = 10;
  	solve();

  	// output
	for (int i = 0; i < n; ++i)
	{
		for (int j = 0; j < n; ++j)
		{
			sys_write_int(c[i][j]);
			sys_write_str(" ");
	    }
	    sys_write_str("\n");
	}

	return 0;
}
