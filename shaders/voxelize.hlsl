#include "constants.hlsl"
#include "structs.hlsl"
#include "constantBuffers.hlsl"
#include "samplers.hlsl"
#include "common.hlsl"
#include "cascadedShadow.hlsl"
#include "voxelUtils.hlsl"
/*
外部slotRootParameter[(UINT)RootParam::RenderResources].InitAsConstants(MaxNumConstants, (UINT)RootParam::RenderResources, 0);
实际定义了32个32位常量的位置，这里只使用了2个32位常量位而已
*/
struct Resources
{
    uint VoxelIndex;
    uint ShadowMapTexIndex;
};

//commandList->SetComputeRoot32BitConstants((UINT)RootParam::RenderResources, 2, resources, 0);
ConstantBuffer<Resources> g_Resources : register(b6);
// ConstantBuffer<Resources> g_Resources : register(b6,space0);
//直接光照
float3 DoDirectLighting(Light light, float3 albedo, float3 normal, float metalness, float roughness, float3 L, float3 V)
{
    float Ndotl = max(0.0,dot(normal,L));
    float kd = lerp(float3(1,1,1),float3(0,0,0),metalness);

    // Lambert diffuse BRDF.
    float3 diffuseBRDF = kd * albedo;
    return diffuseBRDF * light.Color.xyz * NdotL;
}
//方向光照
float3 DoDirectionalLight(Light light, float3 albedo, float3 normal, float metalness, float roughness, float3 PositionV)
{
    // Everything is in view space.
    //固定视线方向
    float4 eyePos = { 0, 0, 0, 1 };

    float3 V = normalize(eyePos.xyz - PositionV);
    float3 L = normalize(-light.DirectionVS.xyz);

    return DoDirectLighting(light, albedo, normal, metalness, roughness, L, V) * light.Intensity;

}

// 计算光衰减
float DoAttenuation(Light light, float distance)
{
    /*
    float smoothstep(float edge0, float edge1, float x)
    {
        // Step 1: 将 x 映射到 [0, 1] 范围
        float t = saturate((x - edge0) / (edge1 - edge0));
    
        // Step 2: 使用 Hermite 曲线进行平滑插值
        // 公式: t * t * (3 - 2 * t)
        return t * t * (3 - 2 * t);
    }
    */
    return 1.0f - smoothstep(light.Range * 0.75f, light.Range, distance);
}

float3 DoPointLight(Light light, float3 albedo, float3 normal, float metalness, float roughness, float3 PositionV)
{
    // Everything is in view space.
    float4 eyePos = { 0, 0, 0, 1 };
    float3 V = normalize(eyePos.xyz - PositionV);
    
    float3 L = light.PositionVS.xyz - PositionV;

    float distance = length(L);
    L /= distance;//单位化

    float attenuation = DoAttenuation(light, distance);

    return DoDirectLighting(light, albedo, normal, metalness, roughness, L, V) * attenuation * light.Intensity;
}
//聚光灯光照效果
float DoSpotCone(Light light, float3 L)
{
    // If the cosine angle of the light's direction 
    // vector and the vector from the light source to the point being 
    // shaded is less than minCos, then the spotlight contribution will be 0.
    float minCos = cos(radians(light.SpotlightAngle));
    // If the cosine angle of the light's direction vector
    // and the vector from the light source to the point being shaded
    // is greater than maxCos, then the spotlight contribution will be 1.
    float maxCos = lerp(minCos, 1, 0.5f);
    float cosAngle = dot(light.DirectionVS.xyz, -L);
    // Blend between the minimum and maximum cosine angles.
    return smoothstep(minCos, maxCos, cosAngle);
}

float3 DoSpotLight(Light light, float3 albedo, float3 normal, float metalness, float roughness, float3 PositionV)
{
    // Everything is in view space.
    float4 eyePos = { 0, 0, 0, 1 };
    
    float3 V = normalize(eyePos.xyz - PositionV);
    
    float3 L = light.PositionVS.xyz - PositionV;
    float distance = length(L);
    L /= distance;
    
    float attenuation = DoAttenuation(light, distance);
    float spotIntensity = DoSpotCone(light, L);
    
    return DoDirectLighting(light, albedo, normal, metalness, roughness, L, V) * attenuation * spotIntensity * light.Intensity;
}

struct VertexIn
{
    float3 Position     : POSITION;
    float3 Normal       : NORMAL;
    float3 Tangent      : TANGENT;
    float3 Bitangent    : BITANGENT;
    float2 TexCoord     : TEXCOORD0;
};

struct GeometryInOut
{
    float3 PositionW    : POSITION0;        // World space position.
    float3 PositionV    : POSITION1;        // View space position.
    float3 NormalW      : NORMAL0;          // World space normal.
    float3 NormalV      : NORMAL1;          // View space normal.
    float3 TangentV     : TANGENT;          // View space tangent.
    float3 BitangentV   : BITANGENT;        // View space bitangent.
    float2 TexCoord     : TEXCOORD0;        // Texture coordinate.
    float4 PositionH    : SV_POSITION;      // Clip space position.
};


GeometryInOut VS(VertexIn vin)
{
    GeometryInOut vout;
    vout.TexCoord = vin.TexCoord;
    
    // transform the position into world space as we will use 
    // this to index into the voxel grid and write to it
    vout.PositionW = mul(float4(vin.Position, 1.0f), g_World).xyz;
    vout.NormalW = mul(vin.Normal, (float3x3) g_World);
    //g_World * g_View 变换顺序：从模型空间到世界空间，再到视图空间
    float4x4 ModelView = mul(g_World, g_View);
    
    // transform to view space
    vout.PositionV = mul(float4(vin.Position, 1.0), ModelView).rgb;
    vout.NormalV = mul(vin.Normal, (float3x3) ModelView);
    vout.TangentV = mul(vin.Tangent, (float3x3) ModelView);
    vout.BitangentV = mul(vin.Bitangent, (float3x3) ModelView);
    
    vout.PositionH = float4(vout.PositionW, 1);
    
    return vout;
}
//maxvertexcount 用于指定几何着色器（Geometry Shader）能够输出的最大顶点数量
[maxvertexcount(3)]
void GS(triangle GeometryInOut input[3], inout TriangleStream<GeometryInOut> triStream)
{
    // Select the greatest component of the face normal
    // 找到最大投影面
    float3 posW0 = input[0].PositionW;
    float3 posW1 = input[1].PositionW;
    float3 posW2 = input[2].PositionW;
    float3 faceNormal = abs(cross(posW1 - posW0, posW2 - posW0)); 

    uint maxI = faceNormal[1] > faceNormal[0] ? 1 : 0;
    maxI = faceNormal[2] > faceNormal[maxI] ? 2 : maxI;


    GeometryInOut output[3];

    for (uint i = 0; i < 3; i++)
    {
        //这里的代码还有一个问题，每个片段在这里都会进行一次最大面的投影选择，最后会导致坐标系不在同一个空间内
        // World space -> Voxel grid space
        //VOXEL_GRID_WORLD_POS 体素网格的世界空间位置
        //VOXEL_GRID_SIZE = 0.1f 体素大小
        //透视除法（Perspective Division）是在光栅化阶段由硬件自动执行的
        output[i].PositionH.xyz = (input[i].PositionH.xyz - VOXEL_GRID_WORLD_POS) / VOXEL_GRID_SIZE;//体素化
        
        // Project to the dominant axis orthogonally
        [flatten]
        switch (maxI)
        {
            //此处是用float4可能是没有意义的，
            case 0:
                output[i].PositionH.xyz = float4(output[i].PositionH.yzx, 1.0f).xyz;
                break;
            case 1:
                output[i].PositionH.xyz = float4(output[i].PositionH.zxy, 1.0f).xyz;
                break;
            case 2:
                //output[i].PositionH.xyz = float4(output[i].PositionH.xyz, 1.0f);
                break;
        }
        
        // Voxel grid space -> Clip space
        // VOXEL_DIMENSION = 256 ，在VXGI.cpp中的视口参数中可以看到，视口VOXEL_DIMENSION*VOXEL_DIMENSION，就是体素空间的正面，
        output[i].PositionH.xy /= VOXEL_DIMENSION;//限制到[0,1]范围内，就光栅化表面一层
        output[i].PositionH.zw = 1;//防止执行透视除法除了，改变了坐标
        // 这里我觉得有很严重的代码问题，后续研究一下
        // 这里的光栅化我觉得没有任何意义，以为他处理的也只是坐标值，而后续PS里没有使用这个，是直接计算的体素索引，
        // 那么这里的计算就是多余的，直接在PS里计算体素索引就行了，后续实验一下
        output[i].PositionW = input[i].PositionW;
        output[i].PositionV = input[i].PositionV;
        output[i].NormalW = input[i].NormalW;
        output[i].NormalV = input[i].NormalV;
        output[i].TangentV = input[i].TangentV;
        output[i].BitangentV = input[i].BitangentV;
        output[i].TexCoord = input[i].TexCoord;
        
        triStream.Append(output[i]);
    }
}

void PS(GeometryInOut pin)
{
    //ResourceDescriptorHeap是HLSL的新特性，需要在GPU端声明好着色器可见的资源描述符堆，并用SetDescriptorHeaps添加到命令列表中
    RWStructuredBuffer<Voxel> voxels = ResourceDescriptorHeap[g_Resources.VoxelIndex];//此处使用，以为缓冲区存储三维数据
    float4 albedoCol = GetAlbedo(pin.TexCoord);
    float3 albedo = albedoCol.rgb;
    float alpha = albedoCol.a;

    float3 normal = GetNormal(pin.NormalV, pin.TangentV, pin.BitangentV, pin.TexCoord);
    float metalness = GetMetalness(pin.TexCoord);
    float roughness = GetRoughness(pin.TexCoord);
    // VOXEL_DIMENSION = 256 ，在VXGI.cpp中的视口参数中可以看到，视口VOXEL_DIMENSION*VOXEL_DIMENSION，就是体素空间的正面
    // float3 positionW = pin.PositionW / VOXEL_DIMENSION / VOXEL_GRID_SIZE;
    // float3 positionW = pin.PositionW / (VOXEL_DIMENSION * VOXEL_GRID_SIZE);
    float3 positionW = pin.PositionW / VOXEL_GRID_SIZE / VOXEL_DIMENSION;//先转换为体素单位，再转换到以VOXEL_DIMENSION为单位的空间内
    /*
        positionW.x * 0.5 + 0.5f：将X坐标从[-1, 1]范围映射到[0, 1]范围
        positionW.y * -0.5 + 0.5f：将Y坐标先翻转（乘以-0.5）再映射到[0, 1]范围（注意这里有个负号，所以Y轴方向被翻转了）
        positionW.z * 0.5 + 0.5f：将Z坐标从[-1, 1]范围映射到[0, 1]范围
    */
    uint3 texIndex = uint3(
        (positionW.x * 0.5 + 0.5f) * VOXEL_DIMENSION,
        (positionW.y * -0.5 + 0.5f) * VOXEL_DIMENSION,
        (positionW.z * 0.5 + 0.5f) * VOXEL_DIMENSION
    );//将坐标最终映为[0,256]范围内的体素索引    
    
        if (all(texIndex < VOXEL_DIMENSION) && all(texIndex >= 0))//超出体素范围直接截断
        {
            float shadowFactor = 1.0f;
            // 级联阴影计算，后期注意级联阴影实现的流程是什么样的，这里暂时先不看了
            int cascadeIndex = GetCascadeIndex(pin.PositionV);
            if (cascadeIndex != -1)
            {
                Texture2DArray shadowMap = ResourceDescriptorHeap[g_Resources.ShadowMapTexIndex];

                float4 positionW = mul(float4(pin.PositionV, 1.0f), g_InvView);
                float4 positionH = mul(positionW, g_ShadowData.LightViewProj[cascadeIndex]);
                positionH.xyz /= positionH.w;

                float depth = positionH.z;

                // NDC space to texture space
                // [-1, 1] -> [0, 1] and flip the y-coord
                float2 shadowMapUV = (positionH.xy + 1.0f) * 0.5f;
                shadowMapUV.y = 1.0f - shadowMapUV.y;
                //此处使用的是基于cascade的硬阴影技术
                shadowFactor = shadowMap.SampleCmpLevelZero(g_SamplerShadow, float3(shadowMapUV, cascadeIndex), depth).r;
            }

            float3 directLighting = float3(0, 0, 0);

            [loop] // Unrolling produces strange behaviour so adding the [loop] attribute.
            for (int i = 0; i < 1; i++)
            {
                // Skip lights that are not enabled.
                if (!LightCB[i].Enabled)
                    continue;
                // Skip point and spot lights that are out of range of the point being shaded.
                if (LightCB[i].Type != DIRECTIONAL_LIGHT &&
                    length(LightCB[i].PositionVS.xyz - pin.PositionV) > LightCB[i].Range)
                    continue;
                switch (LightCB[i].Type)
                {
                case DIRECTIONAL_LIGHT:
                    directLighting += DoDirectionalLight(LightCB[i], albedo, normal, metalness, roughness, pin.PositionV) * shadowFactor;
                    break;
                case POINT_LIGHT:
                    directLighting += DoPointLight(LightCB[i], albedo, normal, metalness, roughness, pin.PositionV);
                    break;
                case SPOT_LIGHT:
                    directLighting += DoSpotLight(LightCB[i], albedo, normal, metalness, roughness, pin.PositionV);
                    break;
                default:
                    break;
                }
            }

            uint voxelIndex = Flatten(texIndex, VOXEL_DIMENSION);
            // InterlockedAdd 原子加法
            InterlockedAdd(voxels[voxelIndex].Radiance, ConvFloat4ToUINT64(float4(directLighting * 50.0, alpha * 255.0)));
            InterlockedAdd(voxels[voxelIndex].Normal, ConvFloat4ToUINT64(float4(pin.NormalW * 50.0, 1)));
            // ImageAtomicUINT64Avg(voxels, Flatten(texIndex), float4(directLighting, alpha));
        }
}

/*
你的观察很敏锐！确实可以只保留一种方式，而且简化设计会更合理。
当前设计存在概念不一致的问题：GS做了投影变换来控制光栅化，但PS中又用原始坐标重新计算体素索引。我们可以简化这个流程。
简化方案一：移除GS中的复杂投影（推荐）
[maxvertexcount(3)]
void GS(triangle GeometryInOut input[3], inout TriangleStream<GeometryInOut> triStream)
{
    // 直接传递原始三角形，不做投影变换
    for (uint i = 0; i < 3; i++)
    {
        GeometryInOut output = input[i];
        
        // 直接将世界坐标转换到体素网格的NDC空间
        // 假设体素网格位于原点，大小固定
        float3 voxelSpacePos = (input[i].PositionW - VOXEL_GRID_WORLD_POS) / VOXEL_GRID_SIZE;
        
        // 映射到[-1, 1]的NDC空间用于光栅化
        output.PositionH.xyz = voxelSpacePos;
        output.PositionH.xy /= VOXEL_DIMENSION;  // 简化：只考虑XY平面
        output.PositionH.zw = 1;
        
        triStream.Append(output);
    }
}



简化方案二：完全在PS中计算（更简单）
// VS保持简单转换
GeometryInOut VS(VertexIn vin)
{
    GeometryInOut vout;
    vout.PositionW = mul(float4(vin.Position, 1.0f), g_World).xyz;
    vout.PositionH = float4(vout.PositionW, 1);  // 直接传递世界坐标
    // ... 其他属性
    return vout;
}

[maxvertexcount(3)]
void GS(triangle GeometryInOut input[3], inout TriangleStream<GeometryInOut> triStream)
{
    // 简单传递，不做特殊处理
    for (uint i = 0; i < 3; i++)
    {
        // 直接传递，让光栅化在默认投影平面上进行
        // 这相当于在XY平面上光栅化
        input[i].PositionH.xy = input[i].PositionW.xy;
        input[i].PositionH.zw = 1;
        triStream.Append(input[i]);
    }
}

void PS(GeometryInOut pin)
{
    // 在PS中计算体素索引
    float3 voxelCoord = (pin.PositionW - VOXEL_GRID_WORLD_POS) / VOXEL_GRID_SIZE;
    
    // 确保在体素网格范围内
    if (all(voxelCoord >= 0) && all(voxelCoord < VOXEL_DIMENSION))
    {
        uint3 texIndex = uint3(voxelCoord);
        // ... 写入体素
    }
}
*/