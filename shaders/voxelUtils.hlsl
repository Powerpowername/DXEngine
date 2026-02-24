#include "constants.hlsl"
#include "utils.hlsl"

#define VOXEL_OFFSET_CORRECTION_FACTOR 5
#define NUM_STEPS 100

uint Flatten(uint3 texCoord, uint dim)
{
    return texCoord.x * dim * dim + 
           texCoord.y * dim + 
           texCoord.z;
}

uint3 Unflatten(uint i, uint dim)
{
    uint3 coord;
    
    coord[0] = i % dim;
    i /= dim;
    
    coord[1] = i % dim;
    i /= dim;
    
    coord[2] = i % dim;
    
    return coord;
}

// ref: https://xeolabs.com/pdfs/OpenGLInsights.pdf Chapter 22
uint64_t ConvFloat4ToUINT64(float4 val)
{
    return (uint64_t(val.w) & 0x0000FFFF) << 48U | (uint64_t(val.z) & 0x0000FFFF) << 32U | (uint64_t(val.y) & 0x0000FFFF) << 16U | (uint64_t(val.x) & 0x0000FFFF);
}

float4 ConvUINT64ToFloat4(uint64_t val)
{
    float4 re = float4(float((val & 0x0000FFFF)), float((val & 0xFFFF0000) >> 16U), float((val & 0xFFFF00000000) >> 32U), float((val & 0xFFFF000000000000) >> 48U));
    return clamp(re, float4(0.0, 0.0, 0.0, 0.0), float4(65535.0, 65535.0, 65535.0, 65535.0));
}

void ImageAtomicUINT64Avg(RWStructuredBuffer<Voxel> voxels, uint index, float4 val)
{
    val.rgb *= 65535.0f;
    uint64_t newVal = ConvFloat4ToUINT64(val);
    
    uint64_t prevStoredVal = 0;
    uint64_t curStoredVal;
    
    InterlockedCompareExchange(voxels[index].Radiance, prevStoredVal, newVal, curStoredVal);
    while (curStoredVal != prevStoredVal)
    {
        prevStoredVal = curStoredVal;
        float4 rval = ConvUINT64ToFloat4(curStoredVal);
        rval.xyz *= rval.w;
        
        float4 curValF = rval + val;
        curValF.xyz /= curValF.w;
        
        newVal = ConvFloat4ToUINT64(curValF);
        InterlockedCompareExchange(voxels[index].Radiance, prevStoredVal, newVal, curStoredVal);
    }
}

uint3 WorldPosToVoxelIndex(float3 position)
{
    position = position / VOXEL_DIMENSION / VOXEL_GRID_SIZE;
    
    return uint3(
        (position.x * 0.5 + 0.5f) * VOXEL_DIMENSION,
        (position.y * -0.5 + 0.5f) * VOXEL_DIMENSION,
        (position.z * 0.5 + 0.5f) * VOXEL_DIMENSION
    );
}

float4 TraceCone(Texture3D<float4> voxels, SamplerState linearSampler, float3 pos, float3 N, float3 direction, float aperture)
{
    
}