#include <cuda_runtime.h>

extern "C" __global__ void dent_spmv_csr_kernel(
    int rows, const int* row_ptr, const int* col_idx,
    const double* values, const double* x, double* y)
{
    const int row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= rows) return;
    double sum=0.0;
    for (int k=row_ptr[row]; k<row_ptr[row+1]; ++k) sum += values[k]*x[col_idx[k]];
    y[row]=sum;
}

extern "C" __global__ void dent_vector_axpy_kernel(
    int n, double alpha, const double* x, double* y)
{
    const int i=blockIdx.x*blockDim.x+threadIdx.x;
    if(i<n) y[i]+=alpha*x[i];
}

extern "C" __global__ void dent_vector_scale_kernel(
    int n, double alpha, double* x)
{
    const int i=blockIdx.x*blockDim.x+threadIdx.x;
    if(i<n) x[i]*=alpha;
}
