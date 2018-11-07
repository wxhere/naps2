// NAPS2.WIA.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

class CWiaTransferCallback : public IWiaTransferCallback
{
public: // Constructors, destructor
	CWiaTransferCallback() : m_cRef(1) {};
	~CWiaTransferCallback() {};

public: // IWiaTransferCallback
	HRESULT __stdcall TransferCallback(
		LONG                lFlags,
		WiaTransferParams   *pWiaTransferParams) override
	{
		HRESULT hr = S_OK;

		switch (pWiaTransferParams->lMessage)
		{
		case WIA_TRANSFER_MSG_STATUS:
			//...
			break;
		case WIA_TRANSFER_MSG_END_OF_STREAM:
			//...
			SHCreateMemStream(nullptr, 0);
			break;
		case WIA_TRANSFER_MSG_END_OF_TRANSFER:
			//...
			SHCreateMemStream(nullptr, 0);
			break;
		default:
			break;
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
		*ppDestination = SHCreateMemStream(nullptr, 0);
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
	/// For ref counting implementation
	LONG                m_cRef;
};

HRESULT CreateWiaDevice(IWiaDevMgr2 *pWiaDevMgr, BSTR bstrDeviceID, IWiaItem2 **ppWiaDevice)
{
	//
	// Validate arguments
	//
	if (NULL == pWiaDevMgr || NULL == bstrDeviceID || NULL == ppWiaDevice)
	{
		return E_INVALIDARG;
	}

	//
	// Initialize out variables
	//
	*ppWiaDevice = NULL;

	//
	// Create the WIA Device
	//
	HRESULT hr = pWiaDevMgr->CreateDevice(0, bstrDeviceID, ppWiaDevice);

	//
	// Return the result of creating the device
	//
	return hr;
}

HRESULT TransferWiaItem(IWiaItem2 *pIWiaItem2)
{
	// Validate arguments
	if (NULL == pIWiaItem2)
	{
		//_tprintf(TEXT("\nInvalid parameters passed"));
		return E_INVALIDARG;
	}

	// Get the IWiaTransfer interface
	IWiaTransfer *pWiaTransfer = NULL;
	HRESULT hr = pIWiaItem2->QueryInterface(IID_IWiaTransfer, (void**)&pWiaTransfer);
	if (SUCCEEDED(hr))
	{
		// Create our callback class
		CWiaTransferCallback *pWiaClassCallback = new CWiaTransferCallback;
		if (pWiaClassCallback)
		{

			LONG lItemType = 0;
			hr = pIWiaItem2->GetItemType(&lItemType);

			//download all items which have WiaItemTypeTransfer flag set
			if (lItemType & WiaItemTypeTransfer)
			{

				// If it is a folder, do folder download . Hence with one API call, all the leaf nodes of this folder 
				// will be transferred
				if ((lItemType & WiaItemTypeFolder))
				{
					//_tprintf(L"\nI am a folder item");
					hr = pWiaTransfer->Download(WIA_TRANSFER_ACQUIRE_CHILDREN, pWiaClassCallback);
					if (S_OK == hr)
					{
						//_tprintf(TEXT("\npWiaTransfer->Download() on folder item SUCCEEDED"));
					}
					else if (S_FALSE == hr)
					{
						//ReportError(TEXT("\npWiaTransfer->Download() on folder item returned S_FALSE. Folder may not be having child items"), hr);
					}
					else if (FAILED(hr))
					{
						//ReportError(TEXT("\npWiaTransfer->Download() on folder item failed"), hr);
					}
				}


				// If this is an file type, do file download
				else if (lItemType & WiaItemTypeFile)
				{
					hr = pWiaTransfer->Download(0, pWiaClassCallback);
					if (S_OK == hr)
					{
						//_tprintf(TEXT("\npWiaTransfer->Download() on file item SUCCEEDED"));
					}
					else if (S_FALSE == hr)
					{
						//ReportError(TEXT("\npWiaTransfer->Download() on file item returned S_FALSE. File may be empty"), hr);
					}
					else if (FAILED(hr))
					{
						//ReportError(TEXT("\npWiaTransfer->Download() on file item failed"), hr);
					}
				}
			}

			// Release our callback.  It should now delete itself.
			pWiaClassCallback->Release();
			pWiaClassCallback = NULL;
		}
		else
		{
			//ReportError(TEXT("\nUnable to create CWiaTransferCallback class instance"));
		}

		// Release the IWiaTransfer
		pWiaTransfer->Release();
		pWiaTransfer = NULL;
	}
	else
	{
		//ReportError(TEXT("\npIWiaItem2->QueryInterface failed on IID_IWiaTransfer"), hr);
	}
	return hr;
}

extern "C" {

	__declspec(dllexport) HRESULT __stdcall GetDeviceManager(IWiaDevMgr2 **devMgr)
	{
		return CoCreateInstance(CLSID_WiaDevMgr2, NULL, CLSCTX_LOCAL_SERVER, IID_IWiaDevMgr2, (void**)devMgr);
	}

	__declspec(dllexport) HRESULT __stdcall GetDevice(IWiaDevMgr2 *devMgr, BSTR deviceId, IWiaItem2 **device)
	{
		return devMgr->CreateDevice(0, deviceId, device);
	}

	__declspec(dllexport) HRESULT __stdcall GetDeviceProperty(IWiaItem2 *device, int propId, int *value)
	{
		IWiaPropertyStorage *propStorage = NULL;
		HRESULT hr = device->QueryInterface(IID_IWiaPropertyStorage, (void**)&propStorage);
		if (SUCCEEDED(hr))
		{
			PROPSPEC PropSpec[1] = { 0 };
			PROPVARIANT PropVariant[1] = { 0 };
			PropSpec[0].ulKind = PRSPEC_PROPID;
			PropSpec[0].propid = propId;

			hr = propStorage->ReadMultiple(1, PropSpec, PropVariant);
			*value = PropVariant[0].lVal;
			propStorage->Release();
		}
		return hr;
	}

	__declspec(dllexport) HRESULT __stdcall GetDevicePropertyBstr(IWiaItem2 *device, int propId, BSTR *value)
	{
		IWiaPropertyStorage *propStorage = NULL;
		HRESULT hr = device->QueryInterface(IID_IWiaPropertyStorage, (void**)&propStorage);
		if (SUCCEEDED(hr))
		{
			PROPSPEC PropSpec[1] = { 0 };
			PROPVARIANT PropVariant[1] = { 0 };
			PropSpec[0].ulKind = PRSPEC_PROPID;
			PropSpec[0].propid = propId;

			hr = propStorage->ReadMultiple(1, PropSpec, PropVariant);
			*value = PropVariant[0].bstrVal;
			propStorage->Release();
		}
		return hr;
	}

	__declspec(dllexport) HRESULT __stdcall SetDeviceProperty(IWiaItem2 *device, int propId, int value)
	{
		IWiaPropertyStorage *propStorage = NULL;
		HRESULT hr = device->QueryInterface(IID_IWiaPropertyStorage, (void**)&propStorage);
		if (SUCCEEDED(hr))
		{
			PROPSPEC PropSpec[1] = { 0 };
			PROPVARIANT PropVariant[1] = { 0 };
			PropSpec[0].ulKind = PRSPEC_PROPID;
			PropSpec[0].propid = propId;
			PropVariant[0].vt = VT_I4;
			PropVariant[0].lVal = value;

			hr = propStorage->WriteMultiple(1, PropSpec, PropVariant, WIA_IPA_FIRST);
			propStorage->Release();
		}
		return hr;
	}

	__declspec(dllexport) HRESULT __stdcall GetItem(IWiaItem2 *device, BSTR itemId, IWiaItem2 **item)
	{
		//return device->FindItemByName(0, itemId, item);
		LONG itemType = 0;
		HRESULT hr = device->GetItemType(&itemType);
		if (SUCCEEDED(hr))
		{
			if (itemType & WiaItemTypeFolder || itemType & WiaItemTypeHasAttachments)
			{
				IEnumWiaItem2 *enumerator = NULL;
				hr = device->EnumChildItems(NULL, &enumerator);

				if (SUCCEEDED(hr))
				{
					int i = 0;
					while (S_OK == hr)
					{
						IWiaItem2 *childItem = NULL;
						hr = enumerator->Next(1, &childItem, NULL);

						if (S_OK == hr)
						{
							if (i++ == 1)
							{
								*item = childItem;
								break;
							}
							BSTR name;
							hr = GetDevicePropertyBstr(childItem, 4099, &name);
							GetItem(childItem, itemId, item);
							/**item = childItem;
							*//*return hr;*/

							/*hr = GetItem(pChildWiaItem, itemId, item);
							pChildWiaItem->Release();
							pChildWiaItem = NULL;*/
						}
					}
					enumerator->Release();
					enumerator = NULL;
				}
			}
		}
		return hr;
	}

	__declspec(dllexport) HRESULT __stdcall SetItemProperty(IWiaItem2 *item, int propId, int value)
	{
		IWiaPropertyStorage *propStorage = NULL;
		HRESULT hr = item->QueryInterface(IID_IWiaPropertyStorage, (void**)&propStorage);
		if (SUCCEEDED(hr))
		{
			PROPSPEC PropSpec[1] = { 0 };
			PROPVARIANT PropVariant[1] = { 0 };
			PropSpec[0].ulKind = PRSPEC_PROPID;
			PropSpec[0].propid = propId;
			PropVariant[0].vt = VT_I4;
			PropVariant[0].lVal = value;

			hr = propStorage->WriteMultiple(1, PropSpec, PropVariant, WIA_IPA_FIRST);
			propStorage->Release();
		}
		return hr;
	}

	__declspec(dllexport) HRESULT __stdcall StartTransfer(IWiaItem2 *item, IWiaTransfer **transfer)
	{
		return item->QueryInterface(IID_IWiaTransfer, (void**)transfer);
	}

	__declspec(dllexport) HRESULT __stdcall Download(IWiaTransfer *transfer, int flags, void* progressCallback, unsigned char **bytes)
	{
		CWiaTransferCallback *callbackClass = new CWiaTransferCallback;
		if (callbackClass)
		{
			return transfer->Download(flags, callbackClass);
		}
		return S_FALSE;
	}

	__declspec(dllexport) HRESULT __stdcall EndTransfer(IWiaTransfer *transfer)
	{
		transfer->Release();
		return S_OK;
	}

}