#include "constants.hlsl"
#include "structs.hlsl"
#include "samplers.hlsl"
#include "PCSS.hlsl"

#define NUM_CASCADES 4 // 4 cascades

int GetCascadeIndex(float3 positionV)
{
    // camera near plane and 4 cascade ends
    // 这里存储的都是z值
    float ends[] = {
        g_ShadowData.CascadeEnds[0].x,  // 第1个级联结束点
        g_ShadowData.CascadeEnds[0].y,  // 第2个级联结束点
        g_ShadowData.CascadeEnds[0].z,  // 第3个级联结束点
        g_ShadowData.CascadeEnds[0].w,  // 第4个级联结束点
        g_ShadowData.CascadeEnds[1].x,  // 相机远平面（第5个点）
    };

    int cascadeIndex = -1;
    for (int i = NUM_CASCADES - 1; i >= 0; i--)
        if (positionV.z < ends[i + 1])
            cascadeIndex = i;

    return cascadeIndex;
}

float CascadedShadowWithPCSS(Texture2DArray cascadeShadowMap, int cascadeIndex, float3 positionV)
{
    // camera near plane and 4 cascade ends
    float ends[] = {
        g_ShadowData.CascadeEnds[0].x,
        g_ShadowData.CascadeEnds[0].y,
        g_ShadowData.CascadeEnds[0].z,
        g_ShadowData.CascadeEnds[0].w,
        g_ShadowData.CascadeEnds[1].x,
    };

    float shadowFactor = 1.0f;

    if (cascadeIndex != -1)
    {
        shadowFactor = PCSS(cascadeShadowMap, cascadeIndex, positionV);
        
        // current depth value within the transition range,
        // blend between current and the next cascade
        if (cascadeIndex + 1 < NUM_CASCADES)
        {
            float currentCascadeNear = ends[cascadeIndex];
            float currentCascadeFar = ends[cascadeIndex + 1];
            float currentCascadeLength = currentCascadeFar - currentCascadeNear;
            // 这一步是为了动态控制过渡区域的大小，使其与级联长度成比例
            float transitionLength = currentCascadeLength * g_ShadowData.TransitionRatio;
            float transitionStart = currentCascadeFar - transitionLength;
            // 检查当前像素是否进入了过渡区域（级联边界附近）
            if (positionV.z >= transitionStart)
            {
                float nextCascadeShadowFactor = PCSS(cascadeShadowMap, cascadeIndex + 1, positionV);
                float transitionFactor = saturate((positionV.z - transitionStart) / (currentCascadeFar - transitionStart));
                shadowFactor = lerp(shadowFactor, nextCascadeShadowFactor, transitionFactor);
            }
        }
    }
    
    return shadowFactor;
}




