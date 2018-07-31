export void performSum(uniform int a[], uniform int b[], uniform int c[], uniform int N)
{
	foreach (i = 0...N)
	{
		c[i] = a[i] + b[i]; 
	}
}