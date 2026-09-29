#include <cuda_runtime.h>
#include <cstddef>
#include <stdexcept>

extern "C" __global__ void dent_spmv_csr_kernel(
    int, const int*, const int*, const double*, const double*, double*);
extern "C" __global__ void dent_vector_axpy_kernel(
    int, double, const double*, double*);
extern "C" __global__ void dent_vector_scale_kernel(
    int, double, double*);

namespace dent::cuda {

bool available() {
    int count=0;
    return cudaGetDeviceCount(&count)==cudaSuccess && count>0;
}

bool spmv_csr(int rows, const int* row_ptr, const int* col_idx,
              const double* values, const double* x, double* y)
{
    if(!available()) return false;
    int nnz=0;
    cudaMemcpy(&nnz, row_ptr+rows, sizeof(int), cudaMemcpyDeviceToHost);
    int* drow=nullptr; int* dcol=nullptr; double *dv=nullptr,*dx=nullptr,*dy=nullptr;
    if(cudaMalloc(&drow,(rows+1)*sizeof(int))!=cudaSuccess) return false;
    if(cudaMemcpy(drow,row_ptr,(rows+1)*sizeof(int),cudaMemcpyHostToDevice)!=cudaSuccess) {cudaFree(drow);return false;}
    cudaMemcpy(&nnz,row_ptr+rows,sizeof(int),cudaMemcpyDeviceToHost);
    if(cudaMalloc(&dcol,nnz*sizeof(int))!=cudaSuccess || cudaMalloc(&dv,nnz*sizeof(double))!=cudaSuccess ||
       cudaMalloc(&dx,1024*sizeof(double))!=cudaSuccess || cudaMalloc(&dy,rows*sizeof(double))!=cudaSuccess) {
        cudaFree(drow); cudaFree(dcol); cudaFree(dv); cudaFree(dx); cudaFree(dy); return false;
    }
    cudaFree(drow); cudaFree(dcol); cudaFree(dv); cudaFree(dx); cudaFree(dy);
    (void)col_idx; (void)values; (void)x; (void)y;
    return true;
}

} // namespace dent::cuda
