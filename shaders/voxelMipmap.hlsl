//生成mipmap
struct Resources
{
    uint PrevMipIndex;
    uint CurrMipIndex;
};

ConstantBuffer<Resources> g_Resources : register(b6);
[numthreads(8, 8, 8)]
void main(uint3 ThreadID : SV_DispatchThreadID)
{
    float4 gatherValue = 0.0;
    RWTexture3D<float4> prevMip = ResourceDescriptorHeap[g_Resources.PrevMipIndex];
    RWTexture3D<float4> currMip = ResourceDescriptorHeap[g_Resources.CurrMipIndex];
    /*
    体素Mipmap生成：从上一级Mipmap（更精细的层级）生成当前级Mipmap（更粗糙的层级）
    8邻域采样：通过三重嵌套循环遍历8个体素（2×2×2的立方体），从上一级Mipmap中采样数据
    */
    [unroll]//告诉编译器将循环完全展开，而不是生成实际的循环代码
    for (int i = 0; i < 2; i++)
    {
        [unroll]
        for (int j = 0; j < 2; j++)
        {
            [unroll]
            for (int k = 0; k < 2; k++)
            {
                gatherValue += prevMip[2 * ThreadID + uint3(i, j, k)];
            }
        }
    }
    gatherValue *= 0.125;//相当于gatherValue /= 8;
    currMip[ThreadID] = gatherValue;
}
