// NAPS2.WIA.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include <queue>
#include <functional>

class CWiaTransferCallback1 : public IWiaDataCallback
{
public: // Constructors, destructor
	CWiaTransferCallback1(IWiaDataTransfer *transfer) : m_cRef(1), transfer(transfer) {}
	~CWiaTransferCallback1() {};

	HRESULT CALLBACK QueryInterface(REFIID riid, void **ppvObject) override
	{
		// Validate arguments
		if (NULL == ppvObject)
		{
			return E_INVALIDARG;
		}

		// Return the appropropriate interface
		if (IsEqualIID(riid, IID_IUnknown))
		{
			*ppvObject = static_cast<IUnknown*>(this);
		}
		else if (IsEqualIID(riid, IID_IWiaDataCallback))
		{
			*ppvObject = static_cast<IWiaDataCallback*>(this);
		}
		else
		{
			*ppvObject = NULL;
			return (E_NOINTERFACE);
		}

		// Increment the reference count before we return the interface
		reinterpret_cast<IUnknown*>(*ppvObject)->AddRef();
		return S_OK;
	}

	ULONG CALLBACK AddRef() override
	{
		return InterlockedIncrement((long*)&m_cRef);
	}

	ULONG CALLBACK Release() override
	{
		LONG cRef = InterlockedDecrement((long*)&m_cRef);
		if (0 == cRef)
		{
			delete this;
		}
		return cRef;
	}

	HRESULT BandedDataCallback(LONG lMessage, LONG lStatus, LONG lPercentComplete, LONG lOffset, LONG lLength,
		LONG lReserved, LONG lResLength, BYTE* pbBuffer)
	{
		return cancel ? S_FALSE : S_OK;
	}

	void Cancel()
	{
		cancel = true;
	}

	IWiaDataTransfer *transfer;

private:
	bool cancel;
	LONG m_cRef;
	std::queue<IStream*> m_streams;
	std::function<void(LONG, LONG, ULONG64, HRESULT, IStream*)> m_statusCallback;
};

class CWiaTransferCallback2 : public IWiaTransferCallback
{
public: // Constructors, destructor
	CWiaTransferCallback2(bool __stdcall statusCallback(LONG, LONG, ULONG64, HRESULT, IStream*)) : m_cRef(1), m_statusCallback(statusCallback) {};
	~CWiaTransferCallback2() {};

public: // IWiaTransferCallback
	HRESULT __stdcall TransferCallback(
		LONG                lFlags,
		WiaTransferParams   *pWiaTransferParams) override
	{
		HRESULT hr = S_OK;
		IStream *stream = NULL;

		switch (pWiaTransferParams->lMessage)
		{
		case WIA_TRANSFER_MSG_STATUS:
			break;
		case WIA_TRANSFER_MSG_END_OF_STREAM:
			//...
			stream = m_streams.front();
			m_streams.pop();
			break;
		case WIA_TRANSFER_MSG_END_OF_TRANSFER:
			break;
		default:
			break;
		}

		if (!m_statusCallback(
			pWiaTransferParams->lMessage,
			pWiaTransferParams->lPercentComplete,
			pWiaTransferParams->ulTransferredBytes,
			pWiaTransferParams->hrErrorStatus,
			stream))
		{
			hr = S_FALSE;
		}

		if (stream)
		{
			stream->Release();
		}

		return hr;
	}

	HRESULT __stdcall GetNextStream(
		LONG    lFlags,
		BSTR    bstrItemName,
		BSTR    bstrFullItemName,
		IStream **ppDestination)
	{

		HRESULT hr = S_OK;

		//  Return a new stream for this item's data.
		//
		IStream *stream = SHCreateMemStream(nullptr, 0);
		*ppDestination = stream;
		m_streams.push(stream);
		stream->AddRef();
		return hr;
	}

public: // IUnknown
	//... // Etc.

	HRESULT CALLBACK QueryInterface(REFIID riid, void **ppvObject) override
	{
		// Validate arguments
		if (NULL == ppvObject)
		{
			return E_INVALIDARG;
		}

		// Return the appropropriate interface
		if (IsEqualIID(riid, IID_IUnknown))
		{
			*ppvObject = static_cast<IUnknown*>(this);
		}
		else if (IsEqualIID(riid, IID_IWiaTransferCallback))
		{
			*ppvObject = static_cast<IWiaTransferCallback*>(this);
		}
		else
		{
			*ppvObject = NULL;
			return (E_NOINTERFACE);
		}

		// Increment the reference count before we return the interface
		reinterpret_cast<IUnknown*>(*ppvObject)->AddRef();
		return S_OK;
	}

	ULONG CALLBACK AddRef() override
	{
		return InterlockedIncrement((long*)&m_cRef);
	}

	ULONG CALLBACK Release() override
	{
		LONG cRef = InterlockedDecrement((long*)&m_cRef);
		if (0 == cRef)
		{
			delete this;
		}
		return cRef;
	}

private:
	LONG m_cRef;
	std::queue<IStream*> m_streams;
	std::function<bool(LONG, LONG, ULONG64, HRESULT, IStream*)> m_statusCallback;
};

HRESULT DoEnum(IWiaItem *item, IEnumWiaItem **enumerator)
{
	return item->EnumChildItems(enumerator);
}

HRESULT DoEnum(IWiaItem2 *item, IEnumWiaItem2 **enumerator)
{
	return item->EnumChildItems(NULL, enumerator);
}

template<class TItem, class TEnum> HRESULT EnumerateItems(TItem *item, void __stdcall itemCallback(TItem*))
{
	LONG itemType = 0;
	HRESULT hr = item->GetItemType(&itemType);
	if (SUCCEEDED(hr))
	{
		if (itemType & WiaItemTypeFolder || itemType & WiaItemTypeHasAttachments)
		{
			TEnum *enumerator = NULL;
			hr = DoEnum(item, &enumerator);

			if (SUCCEEDED(hr))
			{
				while (S_OK == hr)
				{
					TItem *childItem = NULL;
					hr = enumerator->Next(1, &childItem, NULL);

					if (S_OK == hr)
					{
						itemCallback(childItem);
					}
				}
				if (hr == S_FALSE)
				{
					hr = S_OK;
				}
				enumerator->Release();
				enumerator = NULL;
			}
		}
	}
	return hr;
}

template<class TMgr> HRESULT __stdcall EnumerateDevices(TMgr *deviceManager, void __stdcall deviceCallback(IWiaPropertyStorage*))
{
	IEnumWIA_DEV_INFO *enumDevInfo;
	HRESULT hr = deviceManager->EnumDeviceInfo(WIA_DEVINFO_ENUM_LOCAL, &enumDevInfo);
	if (SUCCEEDED(hr))
	{
		while (hr == S_OK)
		{
			IWiaPropertyStorage *propStorage = NULL;
			hr = enumDevInfo->Next(1, &propStorage, NULL);
			if (hr == S_OK)
			{
				deviceCallback(propStorage);
			}
		}
		if (hr == S_FALSE)
		{
			hr = S_OK;
		}
	}
	return hr;
}

extern "C" {

	__declspec(dllexport) HRESULT __stdcall GetDeviceManager1(IWiaDevMgr **devMgr)
	{
		return CoCreateInstance(CLSID_WiaDevMgr, NULL, CLSCTX_LOCAL_SERVER, IID_IWiaDevMgr, (void**)devMgr);
	}

	__declspec(dllexport) HRESULT __stdcall GetDeviceManager2(IWiaDevMgr2 **devMgr)
	{
		return CoCreateInstance(CLSID_WiaDevMgr2, NULL, CLSCTX_LOCAL_SERVER, IID_IWiaDevMgr2, (void**)devMgr);
	}

	__declspec(dllexport) HRESULT __stdcall GetDevice1(IWiaDevMgr *devMgr, BSTR deviceId, IWiaItem **device)
	{
		return devMgr->CreateDevice(deviceId, device);
	}

	__declspec(dllexport) HRESULT __stdcall GetDevice2(IWiaDevMgr2 *devMgr, BSTR deviceId, IWiaItem2 **device)
	{
		return devMgr->CreateDevice(0, deviceId, device);
	}

	__declspec(dllexport) HRESULT __stdcall SetPropertyInt(IWiaPropertyStorage *propStorage, int propId, int value)
	{
		PROPSPEC PropSpec[1] = { 0 };
		PROPVARIANT PropVariant[1] = { 0 };
		PropSpec[0].ulKind = PRSPEC_PROPID;
		PropSpec[0].propid = propId;
		PropVariant[0].vt = VT_I4;
		PropVariant[0].lVal = value;

		return propStorage->WriteMultiple(1, PropSpec, PropVariant, WIA_IPA_FIRST);
	}

	__declspec(dllexport) HRESULT __stdcall GetPropertyBstr(IWiaPropertyStorage *propStorage, int propId, BSTR *value)
	{
		PROPSPEC PropSpec[1] = { 0 };
		PROPVARIANT PropVariant[1] = { 0 };
		PropSpec[0].ulKind = PRSPEC_PROPID;
		PropSpec[0].propid = propId;

		HRESULT hr = propStorage->ReadMultiple(1, PropSpec, PropVariant);
		*value = PropVariant[0].bstrVal;
		return hr;
	}

	__declspec(dllexport) HRESULT __stdcall GetPropertyInt(IWiaPropertyStorage *propStorage, int propId, int *value)
	{
		PROPSPEC PropSpec[1] = { 0 };
		PROPVARIANT PropVariant[1] = { 0 };
		PropSpec[0].ulKind = PRSPEC_PROPID;
		PropSpec[0].propid = propId;

		HRESULT hr = propStorage->ReadMultiple(1, PropSpec, PropVariant);
		*value = PropVariant[0].lVal;
		return hr;
	}

	__declspec(dllexport) HRESULT __stdcall EnumerateItems1(IWiaItem *item, void __stdcall itemCallback(IWiaItem*))
	{
		return EnumerateItems<IWiaItem, IEnumWiaItem>(item, itemCallback);
	}

	__declspec(dllexport) HRESULT __stdcall EnumerateItems2(IWiaItem2 *item, void __stdcall itemCallback(IWiaItem2*))
	{
		return EnumerateItems<IWiaItem2, IEnumWiaItem2>(item, itemCallback);
	}

	__declspec(dllexport) HRESULT __stdcall StartTransfer1(IWiaItem *item, CWiaTransferCallback1 **callback)
	{
		PROPSPEC PropSpec[2] = { 0 };
		PROPVARIANT PropVariant[2] = { 0 };
		GUID guidOutputFormat = WiaImgFmt_BMP;
		PropSpec[0].ulKind = PRSPEC_PROPID;
		PropSpec[0].propid = WIA_IPA_FORMAT;
		PropSpec[1].ulKind = PRSPEC_PROPID;
		PropSpec[1].propid = WIA_IPA_TYMED;
		PropVariant[0].vt = VT_CLSID;
		PropVariant[0].puuid = &guidOutputFormat;
		PropVariant[1].vt = VT_I4;
		PropVariant[1].lVal = TYMED_ISTREAM;
		IWiaPropertyStorage* propStorage;
		HRESULT hr = item->QueryInterface(IID_IWiaPropertyStorage, (void**)&propStorage);
		hr = propStorage->WriteMultiple(2, PropSpec, PropVariant, WIA_IPA_FIRST);

		IWiaDataTransfer *transfer = NULL;
		hr = item->QueryInterface(IID_IWiaDataTransfer, (void**)&transfer);
		if (hr == S_OK)
		{
			*callback = new CWiaTransferCallback1(transfer);

		}
		return hr;
	}

	__declspec(dllexport) HRESULT __stdcall StartTransfer2(IWiaItem2 *item, IWiaTransfer **transfer)
	{
		return item->QueryInterface(IID_IWiaTransfer, (void**)transfer);
	}

	__declspec(dllexport) HRESULT __stdcall Download1(CWiaTransferCallback1 *callback, void __stdcall statusCallback(LONG, LONG, ULONG64, HRESULT, IStream*))
	{
		STGMEDIUM StgMedium = { 0 };
		StgMedium.tymed = TYMED_ISTREAM;
		return callback->transfer->idtGetData(&StgMedium, callback);
	}

	__declspec(dllexport) HRESULT __stdcall Download2(IWiaTransfer *transfer, bool __stdcall statusCallback(LONG, LONG, ULONG64, HRESULT, IStream*))
	{
		CWiaTransferCallback2 *callbackClass = new CWiaTransferCallback2(statusCallback);
		if (callbackClass)
		{
			return transfer->Download(0, callbackClass);
		}
		return S_FALSE;
	}

	__declspec(dllexport) HRESULT __stdcall EnumerateDevices1(IWiaDevMgr *deviceManager, void __stdcall deviceCallback(IWiaPropertyStorage*))
	{
		return EnumerateDevices(deviceManager, deviceCallback);
	}

	__declspec(dllexport) HRESULT __stdcall EnumerateDevices2(IWiaDevMgr2 *deviceManager, void __stdcall deviceCallback(IWiaPropertyStorage*))
	{
		return EnumerateDevices(deviceManager, deviceCallback);
	}

	__declspec(dllexport) HRESULT __stdcall GetItemPropertyStorage(IUnknown *item, IWiaPropertyStorage** propStorage)
	{
		return item->QueryInterface(IID_IWiaPropertyStorage, (void**)propStorage);
	}

	__declspec(dllexport) HRESULT __stdcall EnumerateProperties(IWiaPropertyStorage *propStorage, void __stdcall propCallback(int, LPOLESTR, VARTYPE))
	{
		IEnumSTATPROPSTG *enumProps;
		HRESULT hr = propStorage->Enum(&enumProps);
		if (SUCCEEDED(hr))
		{
			while (hr == S_OK)
			{
				STATPROPSTG prop;
				hr = enumProps->Next(1, &prop, NULL);
				if (hr == S_OK)
				{
					propCallback(prop.propid, prop.lpwstrName, prop.vt);
				}
			}
			if (hr == S_FALSE)
			{
				hr = S_OK;
			}
		}
		return hr;
	}

	__declspec(dllexport) HRESULT __stdcall SelectDevice1(IWiaDevMgr *deviceManager, HWND hwnd, LONG deviceType, LONG flags, BSTR *deviceId, IWiaItem **device)
	{
		return deviceManager->SelectDeviceDlg(hwnd, deviceType, flags, deviceId, device);
	}

	__declspec(dllexport) HRESULT __stdcall SelectDevice2(IWiaDevMgr2 *deviceManager, HWND hwnd, LONG deviceType, LONG flags, BSTR *deviceId, IWiaItem2 **device)
	{
		return deviceManager->SelectDeviceDlg(hwnd, deviceType, flags, deviceId, device);
	}
}