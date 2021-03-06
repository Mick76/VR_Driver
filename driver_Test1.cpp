#include <d3d11.h>
#include <d3dcompiler.h>
#include "openvr_driver.h"

#include <fstream>

using namespace std;

static vr::ETrackedPropertyError(__thiscall* Org_WritePropertyBatch)(vr::IVRProperties* thisptr, vr::PropertyContainerHandle_t ulContainerHandle, vr::PropertyWrite_t* pBatch, uint32_t unBatchEntryCount);

//-----------------------------------------------------------------------------
// Settings
//-----------------------------------------------------------------------------
static const char* const k_pch_VirtualDisplay_Section = "driver_virtual_display";
static const char* const k_pch_VirtualDisplay_SerialNumber_String = "serialNumber";
static const char* const k_pch_VirtualDisplay_ModelNumber_String = "modelNumber";
static const char* const k_pch_VirtualDisplay_AdditionalLatencyInSeconds_Float = "additionalLatencyInSeconds";
static const char* const k_pch_VirtualDisplay_DisplayWidth_Int32 = "displayWidth";
static const char* const k_pch_VirtualDisplay_DisplayHeight_Int32 = "displayHeight";
static const char* const k_pch_VirtualDisplay_DisplayRefreshRateNumerator_Int32 = "displayRefreshRateNumerator";
static const char* const k_pch_VirtualDisplay_DisplayRefreshRateDenominator_Int32 = "displayRefreshRateDenominator";
static const char* const k_pch_VirtualDisplay_AdapterIndex_Int32 = "adapterIndex";

D3D_DRIVER_TYPE         m_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL       m_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device* m_device = nullptr;
ID3D11DeviceContext* m_deviceContext = nullptr;

void InitializeDirectX()
{
	HRESULT hr = S_OK;

	unsigned int createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE, //rapido
		D3D_DRIVER_TYPE_WARP, //combinacion
		D3D_DRIVER_TYPE_REFERENCE, //lento
	};
	unsigned int numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
	};
	unsigned int numFeatureLevels = ARRAYSIZE(featureLevels);

	for (unsigned int driverTypeIndex = 0; driverTypeIndex < numDriverTypes; ++driverTypeIndex) {
		m_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDevice(NULL,
							   m_driverType,
							   NULL,
							   createDeviceFlags,
							   featureLevels,
							   numFeatureLevels,
							   D3D11_SDK_VERSION,
							   &m_device,
							   &m_featureLevel,
							   &m_deviceContext);
		if (SUCCEEDED(hr))
		{
			ofstream myfile;
			myfile.open("C:\\Users\\bluem\\Desktop\\LogDirectx.txt");
			myfile << "directx Works";
			myfile << "\n";
			myfile.close();
			break;
		}
		else
		{
			ofstream myfile;
			myfile.open("C:\\Users\\bluem\\Desktop\\LogDirectx.txt");
			myfile << "directx dont Work";
			myfile << "\n";
			myfile.close();
		}

	}
}

class CVRDisplay : public vr::ITrackedDeviceServerDriver, public vr::IVRDisplayComponent, public vr::IVRVirtualDisplay
{
public:
	CVRDisplay();
	virtual ~CVRDisplay();

	// ITrackedDeviceServerDriver
	virtual vr::EVRInitError Activate(uint32_t unObjectId) override;

	virtual void Deactivate() override;

	virtual void* GetComponent(const char* pchComponentNameAndVersion) override;

	virtual void EnterStandby() override;

	virtual void DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize) override;

	virtual vr::DriverPose_t GetPose() override;

	std::string GetSerialNumber()
	{
		return m_rchSerialNumber;
	}

	//IVRVirtualDisplay
	virtual void Present(const vr::PresentInfo_t* pPresentInfo, uint32_t unPresentInfoSize) override;

	virtual void WaitForPresent() override;

	virtual bool GetTimeSinceLastVsync(float* pfSecondsSinceLastVsync, uint64_t* pulFrameCounter) override;

	//display
	virtual void GetWindowBounds(int32_t* pnX, int32_t* pnY, uint32_t* pnWidth, uint32_t* pnHeight) override;

	virtual bool IsDisplayOnDesktop() override;

	virtual bool IsDisplayRealDisplay() override;

	virtual void GetRecommendedRenderTargetSize(uint32_t* pnWidth, uint32_t* pnHeight) override;

	virtual void GetEyeOutputViewport(vr::EVREye eEye, uint32_t* pnX, uint32_t* pnY, uint32_t* pnWidth, uint32_t* pnHeight) override;

	virtual void GetProjectionRaw(vr::EVREye eEye, float* pfLeft, float* pfRight, float* pfTop, float* pfBottom) override;

	virtual vr::DistortionCoordinates_t ComputeDistortion(vr::EVREye eEye, float fU, float fV) override;


private:
	uint32_t m_unObjectId;
	char m_rchSerialNumber[1024];
	char m_rchModelNumber[1024];
	float m_flAdditionalLatencyInSeconds;
	std::string m_sSerialNumber;
	std::string m_sModelNumber;

	int32_t nDisplayWidth;
	int32_t nDisplayHeight;
	float m_flIPD;

	string m_hasEnterPresent = "has not entered present"; //to check at the end of the program if it entered the present function
};

CVRDisplay::CVRDisplay()
{
	m_unObjectId = vr::k_unTrackedDeviceIndexInvalid;

	vr::VRSettings()->GetString(k_pch_VirtualDisplay_Section, vr::k_pch_Null_SerialNumber_String, m_rchSerialNumber, sizeof(m_rchSerialNumber));
	vr::VRSettings()->GetString(k_pch_VirtualDisplay_Section, vr::k_pch_Null_ModelNumber_String, m_rchModelNumber, sizeof(m_rchModelNumber));
	m_sSerialNumber = m_rchSerialNumber;
	m_sModelNumber = m_rchModelNumber;

	m_flAdditionalLatencyInSeconds = max( 0.0f, vr::VRSettings()->GetFloat( k_pch_VirtualDisplay_Section, k_pch_VirtualDisplay_AdditionalLatencyInSeconds_Float ) );

	nDisplayWidth = vr::VRSettings()->GetInt32( k_pch_VirtualDisplay_Section, k_pch_VirtualDisplay_DisplayWidth_Int32 );
	nDisplayHeight = vr::VRSettings()->GetInt32( k_pch_VirtualDisplay_Section, k_pch_VirtualDisplay_DisplayHeight_Int32 );

	int32_t nDisplayRefreshRateNumerator = vr::VRSettings()->GetInt32( k_pch_VirtualDisplay_Section, k_pch_VirtualDisplay_DisplayRefreshRateNumerator_Int32 );
	int32_t nDisplayRefreshRateDenominator = vr::VRSettings()->GetInt32( k_pch_VirtualDisplay_Section, k_pch_VirtualDisplay_DisplayRefreshRateDenominator_Int32 );

	int32_t nAdapterIndex = vr::VRSettings()->GetInt32( k_pch_VirtualDisplay_Section, k_pch_VirtualDisplay_AdapterIndex_Int32 );

	m_flIPD = vr::VRSettings()->GetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_IPD_Float);
}

CVRDisplay::~CVRDisplay()
{

}

vr::EVRInitError CVRDisplay::Activate(uint32_t unObjectId)
{
	//create file if this gets activated
	ofstream myfile;
	myfile.open("C:\\Users\\bluem\\Desktop\\LogActivate.txt");
	myfile << "Activate works";
	myfile.close();

	m_unObjectId = unObjectId;

	vr::PropertyContainerHandle_t ulContainer = vr::VRProperties()->TrackedDeviceToPropertyContainer(unObjectId);

	vr::VRProperties()->SetStringProperty(ulContainer, vr::Prop_ModelNumber_String, m_sModelNumber.c_str());
	vr::VRProperties()->SetStringProperty(ulContainer, vr::Prop_RenderModelName_String, m_sModelNumber.c_str());

	vr::VRProperties()->SetFloatProperty(ulContainer, vr::Prop_UserIpdMeters_Float, m_flIPD);
	vr::VRProperties()->SetFloatProperty(ulContainer, vr::Prop_UserHeadToEyeDepthMeters_Float, 0.f);
	vr::VRProperties()->SetFloatProperty(ulContainer, vr::Prop_DisplayFrequency_Float, 0);
	vr::VRProperties()->SetFloatProperty(ulContainer, vr::Prop_SecondsFromVsyncToPhotons_Float, m_flAdditionalLatencyInSeconds);

	// return a constant that's not 0 (invalid) or 1 (reserved for Oculus)
	vr::VRProperties()->SetUint64Property(ulContainer, vr::Prop_CurrentUniverseId_Uint64, 2);
	vr::VRProperties()->SetBoolProperty(ulContainer, vr::Prop_IsOnDesktop_Bool, false);

	vr::VRProperties()->SetUint64Property(ulContainer, vr::Prop_GraphicsAdapterLuid_Uint64, 0);


	return vr::VRInitError_None;
}

void CVRDisplay::Deactivate()
{
	//create file if this gets deactivated and if during the test it entered the present function
	ofstream myfile;
	myfile.open("C:\\Users\\bluem\\Desktop\\LogDeactivate.txt");
	myfile << "Deactivate works\n";
	myfile << m_hasEnterPresent; //to check if it entered the present function
	myfile << "\n";
	myfile.close();
	m_unObjectId = vr::k_unTrackedDeviceIndexInvalid;
}

void* CVRDisplay::GetComponent(const char* pchComponentNameAndVersion)
{
	//create file to check if it found de component
	if (!_stricmp(pchComponentNameAndVersion, vr::IVRVirtualDisplay_Version))
	{
		ofstream myfile;
		myfile.open("C:\\Users\\bluem\\Desktop\\LogFoundComponent.txt", std::ofstream::app);
		myfile << "Found the component: ";
		myfile << pchComponentNameAndVersion;
		myfile << "\n";
		myfile.close();
		return static_cast<vr::IVRVirtualDisplay*>(this);
	}
	else if (!_stricmp(pchComponentNameAndVersion, vr::IVRDisplayComponent_Version))
	{
		ofstream myfile;
		myfile.open("C:\\Users\\bluem\\Desktop\\LogFoundComponent.txt", std::ofstream::app);
		myfile << "Found the component: ";
		myfile << pchComponentNameAndVersion;
		myfile << "\n";
		myfile.close();
		return static_cast<vr::IVRDisplayComponent*>(this);
	}
	ofstream myfile;
	myfile.open("C:\\Users\\bluem\\Desktop\\LogFoundComponent.txt", std::ofstream::app);
	myfile << "This is not the component: ";
	myfile << pchComponentNameAndVersion;
	myfile << "\n";
	myfile.close();
	return NULL;
}

void CVRDisplay::EnterStandby()
{
}

void CVRDisplay::DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize)
{
	if (unResponseBufferSize >= 1)
		pchResponseBuffer[0] = 0;
}

vr::DriverPose_t CVRDisplay::GetPose()
{
	vr::DriverPose_t pose = { 0 };
	pose.poseIsValid = true;
	pose.result = vr::TrackingResult_Running_OK;
	pose.deviceIsConnected = true;
	pose.qWorldFromDriverRotation.w = 1;
	pose.qWorldFromDriverRotation.x = 0;
	pose.qWorldFromDriverRotation.y = 0;
	pose.qWorldFromDriverRotation.z = 0;
	pose.qDriverFromHeadRotation.w = 1;
	pose.qDriverFromHeadRotation.x = 0;
	pose.qDriverFromHeadRotation.y = 0;
	pose.qDriverFromHeadRotation.z = 0;

	return pose;
}

void CVRDisplay::Present(const vr::PresentInfo_t* pPresentInfo, uint32_t unPresentInfoSize)
{
	m_hasEnterPresent = "Enter present"; //log this string on Deactivate()

	/*
	ID3D11Texture2D* pTexture;
	HRESULT hr;
	hr = m_device->OpenSharedResource((HANDLE)backbufferTextureHandle, __uuidof(ID3D11Texture2D), (void**)& pTexture);

	D3D11_TEXTURE2D_DESC desc;
	pTexture->GetDesc(&desc);
	*/
}

void CVRDisplay::WaitForPresent()
{
	m_hasEnterPresent = "Enter present"; //log this string on Deactivate()

}

bool CVRDisplay::GetTimeSinceLastVsync(float* pfSecondsSinceLastVsync, uint64_t* pulFrameCounter)
{
	m_hasEnterPresent = "Enter present"; //log this string on Deactivate()

	return true;
}

//display
void CVRDisplay::GetWindowBounds(int32_t* pnX, int32_t* pnY, uint32_t* pnWidth, uint32_t* pnHeight)
{
	*pnX = 20;
	*pnY = 20;
	*pnWidth = nDisplayWidth;
	*pnHeight = nDisplayHeight;
}

bool CVRDisplay::IsDisplayOnDesktop()
{
	return true;
}

bool CVRDisplay::IsDisplayRealDisplay()
{
	return false;
}

void CVRDisplay::GetRecommendedRenderTargetSize(uint32_t* pnWidth, uint32_t* pnHeight)
{
	*pnWidth = nDisplayWidth*0.5f;
	*pnHeight = nDisplayHeight*0.5f;
}

void CVRDisplay::GetEyeOutputViewport(vr::EVREye eEye, uint32_t* pnX, uint32_t* pnY, uint32_t* pnWidth, uint32_t* pnHeight)
{
	*pnY = 0;
	*pnWidth = nDisplayWidth / 2;
	*pnHeight = nDisplayHeight;

	if (eEye == vr::Eye_Left)
	{
		*pnX = 0;
	}
	else
	{
		*pnX = nDisplayWidth / 2;
	}
}

void CVRDisplay::GetProjectionRaw(vr::EVREye eEye, float* pfLeft, float* pfRight, float* pfTop, float* pfBottom)
{
	*pfLeft = -1.0;
	*pfRight = 1.0;
	*pfTop = -1.0;
	*pfBottom = 1.0;
}

vr::DistortionCoordinates_t CVRDisplay::ComputeDistortion(vr::EVREye eEye, float fU, float fV)
{
	vr::DistortionCoordinates_t coordinates;
	coordinates.rfBlue[0] = fU;
	coordinates.rfBlue[1] = fV;
	coordinates.rfGreen[0] = fU;
	coordinates.rfGreen[1] = fV;
	coordinates.rfRed[0] = fU;
	coordinates.rfRed[1] = fV;
	return coordinates;
}


//-----------------------------------------------------------------------------
// Purpose: Server interface implementation.
//-----------------------------------------------------------------------------
class CServerDriver_DisplayRedirect : public vr::IServerTrackedDeviceProvider
{
public:
	CServerDriver_DisplayRedirect()
		: m_pDisplayRedirectLatest(NULL)
	{}

	virtual vr::EVRInitError Init(vr::IVRDriverContext* pContext) override;
	virtual void Cleanup() override;
	virtual const char* const* GetInterfaceVersions() override
	{
		return vr::k_InterfaceVersions;
	}
	virtual const char* GetTrackedDeviceDriverVersion()
	{
		return vr::ITrackedDeviceServerDriver_Version;
	}
	virtual void RunFrame() override {}
	virtual bool ShouldBlockStandbyMode() override { return false; }
	virtual void EnterStandby() override {}
	virtual void LeaveStandby() override {}

private:
	CVRDisplay* m_pDisplayRedirectLatest;
};

vr::EVRInitError CServerDriver_DisplayRedirect::Init(vr::IVRDriverContext* pContext)
{
	VR_INIT_SERVER_DRIVER_CONTEXT(pContext);

	m_pDisplayRedirectLatest = new CVRDisplay();

	vr::VRServerDriverHost()->TrackedDeviceAdded(m_pDisplayRedirectLatest->GetSerialNumber().c_str(),
												 vr::TrackedDeviceClass_DisplayRedirect,
												 m_pDisplayRedirectLatest);

	return vr::VRInitError_None;
}

void CServerDriver_DisplayRedirect::Cleanup()
{
	delete m_pDisplayRedirectLatest;
	m_pDisplayRedirectLatest = NULL;

	VR_CLEANUP_SERVER_DRIVER_CONTEXT();
}

CServerDriver_DisplayRedirect g_serverDriverDisplayRedirect;

//-----------------------------------------------------------------------------
// Purpose: Entry point for vrserver when loading drivers.
//-----------------------------------------------------------------------------
extern "C" __declspec(dllexport)
void* HmdDriverFactory(const char* pInterfaceName, int* pReturnCode)
{
	if (0 == strcmp(vr::IServerTrackedDeviceProvider_Version, pInterfaceName))
	{
		return &g_serverDriverDisplayRedirect;
	}

	if (pReturnCode)
		* pReturnCode = vr::VRInitError_Init_InterfaceNotFound;

	return NULL;
}