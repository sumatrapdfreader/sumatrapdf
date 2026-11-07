// IRawElementProvider* method bodies shared by the Sumatra UIA providers

#include <uiautomationcore.h>
#include <uiautomationcoreapi.h>

// out-param for "nothing to return": GetRuntimeId of a top-level element,
// GetEmbeddedFragmentRoots (no other roots), get_HostRawElementProvider
template <typename T>
inline HRESULT UiaReturnNull(T** pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }
    *pRetVal = nullptr;
    return S_OK;
}

// UseComThreading makes UIA marshal every provider call onto the thread the
// provider was created on (the UI thread, an STA via OleInitialize). Without
// it calls arrive on RPC threads and race document teardown: a GetText() on
// an RPC thread called a pure virtual on an engine the UI thread was deleting.
inline HRESULT UiaProviderOptions(ProviderOptions* pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }
    *pRetVal = ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading;
    return S_OK;
}

// providers that share the canvas provider's area and fragment root
template <typename T>
inline HRESULT UiaRootRect(T* root, UiaRect* pRetVal) {
    if (!root) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    return root->get_BoundingRectangle(pRetVal);
}

template <typename T>
inline HRESULT UiaRootFragmentRoot(T* root, IRawElementProviderFragmentRoot** pRetVal) {
    if (pRetVal == nullptr) {
        return E_POINTER;
    }
    *pRetVal = root;
    if (!root) {
        return UIA_E_ELEMENTNOTAVAILABLE;
    }
    root->AddRef();
    return S_OK;
}
