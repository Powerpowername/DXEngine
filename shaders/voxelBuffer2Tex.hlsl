#include "structs.hlsl"
#include "constants.hlsl"
#include "voxelUtils.hlsl"

//将3D体素数据从缓冲区转换到3D纹理中
struct Resources
{
    uint BufferIndex;//体素缓冲区索引
    uint TexIndex;//3D纹理索引
};

ConstantBuffer<Resources> g_Resources : register(b6);
//numthreads语法是只有计算着色器才会使用的语法
[numthreads(8, 8, 8)]
void main(uint3 texCoord : SV_DispatchThreadID)
{
    RWStructuredBuffer<Voxel> buffer = ResourceDescriptorHeap[g_Resources.BufferIndex];//取出描述符堆中描述符对应的资源
    RWTexture3D<float4> tex = ResourceDescriptorHeap[g_Resources.TexIndex];//取出3D纹理资源
    //wait for explaination of VOXEL_DIMENSION
    uint bufferIndex = Flatten(texCoord, VOXEL_DIMENSION);
    float4 color = ConvUINT64ToFloat4(buffer[bufferIndex].Radiance);//获取体素的辐射信息
    
    // reset buffer
    // buffer[bufferIndex].Radiance = 0;

    if(color.a < 0)
    {
        tex[texCoord] = 0;
        return;
    }
    //voxelize.hlsl中将光照值放大了50倍，将alpha从0，1转换到了0，255
    color.rgb /= 50.0;
    color.a /= 255.0;

    // avoid very bright spot when alpha is low
    color.a = max(color.a, 1); 
    color /= color.a;

    tex[texCoord] = color;

    // color.rgb /= 255.0;
    // color.rgb *= VOXEL_COMPRESS_COLOR_RANGE;
    
    // color /= color.a;
    
    // gVoxelizerRadiance[ThreadID] = convVec4ToRGBA8(color * 255.0);

}

