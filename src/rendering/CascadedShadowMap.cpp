#include "pch.h"
#include "CascadedShadowMap.h"
#include "RenderingSettings.h"


extern RenderingSettings g_RenderingSettings;

CascadedShadowMap::CascadedShadowMap(std::shared_ptr<DxContext> dxContext)
	: m_Device(dxContext->GetDevice())
{
    D3D12_RESOURCE_DESC texDesc{};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;//使用默认的内存对齐方式
    texDesc.Width = SHADOW_MAP_SIZE;
    texDesc.Height = SHADOW_MAP_SIZE;
    texDesc.DepthOrArraySize = 4; // 4个级联
    texDesc.MipLevels = 1;
    texDesc.Format = m_Format;
    texDesc.SampleDesc = {1, 0};
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;//让驱动程序自动选择最优布局
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear{};
    optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	ThrowIfFailed(m_Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear,
		IID_PPV_ARGS(&m_Resource)));
	// 使用DSV视图写入深度值 
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;        

	dsvDesc.Texture2DArray.ArraySize = 1;
	dsvDesc.Texture2DArray.MipSlice = 0;

	// 使用SRV视图读取深度值进行阴影计算
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = 1;
	srvDesc.Texture2DArray.ArraySize = 1;

	for (int i = 0; i < 4; i++)
	{
		m_Dsvs[i] = dxContext->GetDsvHeap().Alloc();
		dsvDesc.Texture2DArray.FirstArraySlice = i;
        m_Device->CreateDepthStencilView(m_Resource.Get(),&dsvDesc,m_Dsvs[i].CPUHandle);
		
        m_Srvs[i] = dxContext->GetCbvSrvUavHeap().Alloc();
		srvDesc.Texture2DArray.FirstArraySlice = i;
		m_Device->CreateShaderResourceView(m_Resource.Get(), &srvDesc, m_Srvs[i].CPUHandle);
	}
	// 创建一个SRV视图，包含整个资源（4个级联），方便在着色器中一次性访问所有级联
	m_Srvs[4] = dxContext->GetCbvSrvUavHeap().Alloc();
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = 4;
	m_Device->CreateShaderResourceView(m_Resource.Get(), &srvDesc, m_Srvs[4].CPUHandle);
}

void CascadedShadowMap::CalcOrthoProjs(const Camera &camera, const Light &mainLight)
{
	// Calculate each cascade's range 
    // 1. 初始化级联起点为相机近平面
	m_CascadeEnds[0] = camera.GetNearZ();
    // 2. 使用指数增长计算各阶权重
	float expScale[NUM_CASCADES] = {1.0f};//只有expScale[0]被初始化为1，后续的初始化为0
	float expNormalizeFactor = 1.0f;
	for (int i = 1; i < NUM_CASCADES; i++)
	{
        // 指数增长：每级级联的范围是上一级的 CascadeRangeScale 倍
        // 例如：如果 CascadeRangeScale=2，则 expScale = [1, 2, 4, 8...]
		expScale[i] = expScale[i - 1] * g_RenderingSettings.CascadeRangeScale;
		expNormalizeFactor += expScale[i];
	}
    // 3. 归一化因子，使所有权重和为1
	expNormalizeFactor = 1.0f / expNormalizeFactor;
    // 4. 计算实际的级联结束距离（从近到远）
	float percentage = 0.0f;
	for (int i = 0; i < NUM_CASCADES; i++)
	{
		float percentageOffset = expScale[i] * expNormalizeFactor;// 该级联占总距离的比例
		percentage += percentageOffset;// 累积百分比
		m_CascadeEnds[i + 1] = percentage * g_RenderingSettings.MaxShadowDistance;
	}

	XMMATRIX view = camera.GetView();

	// Only the first "main" light casts a shadow.
	XMVECTOR lightDir = XMLoadFloat4(&mainLight.DirectionWS);// 光源方向（世界空间）
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);// 光源的上方向（通常取世界Y轴）

	for (int i = 0; i < NUM_CASCADES; i++)
	{
		float nearZ = m_CascadeEnds[i];// 当前级联开始距离
		float farZ = m_CascadeEnds[i + 1];// 当前级联结束距离
        // 创建过渡区域：除了第一级，都将近平面向前推一点
        // 这是为了避免级联切换时的"pop"现象（阴影突然变化）
		if (i > 0)
		{
			float cascadeLength = m_CascadeEnds[i] - m_CascadeEnds[i - 1];
			nearZ -= cascadeLength * g_RenderingSettings.CascadeTransitionRatio;
		}

		// Get frustum corner coordinates in world space
        // 为当前级联的近远平面创建一个透视投影
		XMMATRIX proj = XMMatrixPerspectiveFovLH(camera.GetFovY(), camera.GetAspect(), nearZ, farZ);
		XMMATRIX viewProj = XMMatrixMultiply(view, proj);
		XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj); // NDC->世界

		std::vector<XMVECTOR> frustumCornersWS;

        // NDC空间的8个角点：x∈[-1,1], y∈[-1,1], z∈[0,1]（DirectX的NDC）
        // 通过逆变换得到世界空间的角点
		for (int x = 0; x < 2; x++)
		{
			for (int y = 0; y < 2; y++)
			{
				for (int z = 0; z < 2; z++)
				{
                    // 1. 构造NDC空间的坐标[-1,1]，z根据DirectX的NDC范围设置为0或1
					XMVECTOR NDCCoords = XMVectorSet(2.0f * x - 1.0f, 2.0f * y - 1.0f, z, 1.0f);
                    // 2. 从NDC变换到裁剪空间，再变换到世界空间
					auto world = XMVector4Transform(NDCCoords, invViewProj);
                    // 3. 透视除法：从齐次坐标转换到三维坐标
					float w = XMVectorGetW(world);
					frustumCornersWS.push_back(world / w);
				}
			}
		}

		// Calculate the bounding sphere
		// ref: https://zhuanlan.zhihu.com/p/515385379
        //下面的代码目的是为了计算aabb包围盒的，aabb包围盒主要是给平行光源做正射投影用的
		auto nearPlaneDiagonal = frustumCornersWS[6] - frustumCornersWS[0];// 近平面对角线
		float a2 = XMVectorGetX(XMVector3LengthSq(nearPlaneDiagonal));// 对角线长度平方

		auto farPlaneDiagonal = frustumCornersWS[7] - frustumCornersWS[1];// 远平面对角线
		float b2 = XMVectorGetX(XMVector3LengthSq(farPlaneDiagonal));

		float len = farZ - nearZ; // 视锥体深度
		float x = len * 0.5f - (a2 - b2) / (8.0f * len); // 球心沿视线方向的偏移

		auto sphereCenterWS = camera.GetPosition() + camera.GetLook() * (nearZ + x);//确定球心位置
		float sphereRadius = sqrtf(x * x + a2 * 0.25);
		// LOG_INFO("Cascade radius [{}]: {}", i, sphereRadius);

		// for removing edge shimmer effect
        // 创建临时光源视图矩阵（原点看向光源反方向）
		XMMATRIX shadowView = XMMatrixLookAtLH(XMVectorSet(0, 0, 0, 1), -lightDir, lightUp);
		XMMATRIX invShadowView = XMMatrixInverse(&XMMatrixDeterminant(shadowView), shadowView);
        /*
        假设：
        纹素大小 = 0.1 单位
        原始球心坐标 = (1.23, 4.56, 7.89)
        步骤1: 除以纹素大小
        (1.23, 4.56, 7.89) ÷ 0.1 = (12.3, 45.6, 78.9)
        步骤2: 向下取整（关键步骤）
        floor(12.3, 45.6, 78.9) = (12, 45, 78)
        步骤3: 乘回纹素大小
        (12, 45, 78) × 0.1 = (1.2, 4.5, 7.8)
        */
        // 通过量化(quantization)将连续的世界坐标对齐到离散的纹素网格上。
        // 计算每个纹素在世界空间中的大小
		float worldUnitsPerTexel = sphereRadius * 2.0 / SHADOW_MAP_SIZE;
		auto vWorldUnitsPerTexel = XMVectorSet(worldUnitsPerTexel, worldUnitsPerTexel, 1, 1);

		auto sphereCenterLS = XMVector3Transform(sphereCenterWS, shadowView);// 转换到光源空间
		sphereCenterLS /= vWorldUnitsPerTexel;// 缩放到纹素空间
		sphereCenterLS = XMVectorFloor(sphereCenterLS);// 向下取整（量化）
		sphereCenterLS *= vWorldUnitsPerTexel;// 转换回世界空间
		sphereCenterWS = XMVector3Transform(sphereCenterLS, invShadowView);// 转换回世界空间
        // 此处我理解sceneRadius是没有用的，因为正射投影和光源的距离是没有用的吧
        // 补：其实还是有用的，XMMatrixLookAtLH因该使用真实的点光源的位置，所以只要足够远就可以了
		float sceneRadius = 50.0f; // TODO: use an actual bounding sphere?这里的注释因该是不对的，平行光源因该没有覆盖整个场景的说法，只要足够高就可以了，不是在物体下面
		float backDistance = sceneRadius + XMVectorGetX(XMVector3Length(sphereCenterWS));

		XMMATRIX lightView = XMMatrixLookAtLH(sphereCenterWS - lightDir * backDistance, sphereCenterWS, lightUp);
		XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(-sphereRadius, sphereRadius, -sphereRadius, sphereRadius, 0, backDistance * 2);
		m_ViewProjMatrix[i] = lightView * lightProj;
		m_CascadeRadius[i] = sphereRadius;
	}
}