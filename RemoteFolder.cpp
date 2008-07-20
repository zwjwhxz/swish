// RemoteFolder.cpp : Implementation of CRemoteFolder

#include "stdafx.h"
#include "remotelimits.h"
#include "RemoteFolder.h"
#include "RemoteEnumIDList.h"
#include "NewConnDialog.h"

#include <ATLComTime.h>

/**
 * Retrieves the class identifier (CLSID) of the object.
 * 
 * @implementing IPersist
 *
 * @param pClsid The location to return the CLSID in.
 */
STDMETHODIMP CRemoteFolder::GetClassID( CLSID* pClsid )
{
	ATLTRACE("CRemoteFolder::GetClassID called\n");

	ATLASSERT( pClsid );
    if (pClsid == NULL)
        return E_POINTER;

	*pClsid = __uuidof(CRemoteFolder);
    return S_OK;
}

/**
 * Assigns an @b absolute PIDL to this folder which we store for later.
 *
 * @implementing IPersistFolder
 *
 * This function is used to tell a folder its place in the system namespace. 
 * if the folder implementation needs to construct a fully qualified PIDL
 * to elements that it contains, the PIDL passed to this method should be 
 * used to construct these.
 *
 * @param pidl The PIDL that specifies the absolute location of this folder.
 */
STDMETHODIMP CRemoteFolder::Initialize( PCIDLIST_ABSOLUTE pidl )
{
	ATLTRACE("CRemoteFolder::Initialize called\n");

	ATLASSERT( pidl != NULL );
    m_pidl = m_PidlManager.Copy( pidl );
	ATLASSERT( m_pidl );
	return S_OK;
}

/*------------------------------------------------------------------------------
 * CRemoteFolder::GetCurFolder() : IPersistFolder2
 * Retrieves the fully qualified PIDL of the folder.
 *----------------------------------------------------------------------------*/
STDMETHODIMP CRemoteFolder::GetCurFolder( __deref_out PIDLIST_ABSOLUTE *ppidl )
{
	ATLTRACE("CRemoteFolder::GetCurFolder called\n");
	ATLASSERT( m_pidl );
	ATLENSURE_RETURN_HR(ppidl, E_POINTER);

	*ppidl = NULL;
	if (m_pidl == NULL)
		return S_FALSE;

	// Make a copy of the PIDL that was passed to Swish in Initialize(pidl)
	*ppidl = m_PidlManager.Copy( m_pidl );
	ATLASSERT( *ppidl );
	if(!*ppidl)
		return E_OUTOFMEMORY;
	
	return S_OK;
}

/*------------------------------------------------------------------------------
 * CRemoteFolder::BindToObject : IShellFolder
 * Subfolder of root folder opened: Create and initialize new CRemoteFolder 
 * COM object to represent subfolder and return its IShellFolder interface.
 *----------------------------------------------------------------------------*/
STDMETHODIMP CRemoteFolder::BindToObject( __in PCUIDLIST_RELATIVE pidl,
										  __in_opt IBindCtx *pbc,
										  __in REFIID riid,
										  __out void** ppvOut )
{
	ATLTRACE("CRemoteFolder::BindToObject called\n");
	(void)pbc;
	*ppvOut = NULL;
	HRESULT hr;

	// We can assume all children of pidl are for RemoteFolder 
	// objects (REMOTEPIDLs) as we have no junction points etc. 
	// Check that the first item in the pidl also a REMOTEPIDL as 
	// this is the remote subfolder/file that we are binding to.
	hr = m_PidlManager.IsValid( pidl );
	ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

	// Create COM object to bind to PIDL
	CComObject<CRemoteFolder> *pRemoteFolder;
	hr = CComObject<CRemoteFolder>::CreateInstance( &pRemoteFolder );
	if (FAILED(hr))
        return hr;

    pRemoteFolder->AddRef();

    // Initialize COM object from PIDL
    PIDLIST_ABSOLUTE pidlBind = ILCombine( m_pidl, pidl );  
    hr = pidlBind ? S_OK : E_OUTOFMEMORY;
    if (SUCCEEDED(hr))
    {
		// Initialise and retrieve requested interface
        hr = pRemoteFolder->Initialize( pidlBind );
        if (SUCCEEDED(hr))
            hr = pRemoteFolder->QueryInterface( riid, ppvOut );
        
		m_PidlManager.Delete( pidlBind );
    }

    pRemoteFolder->Release();

	/* TODO: Not sure if I have done this properly with QueryInterface
	 * From MS: Implementations of BindToObject can optimize any call to 
	 * it by  quickly failing for IID values that it does not support. For 
	 * example, if the Shell folder object of the subitem does not support 
	 * IRemoteComputer, the implementation should return E_NOINTERFACE 
	 * immediately instead of needlessly creating the Shell folder object 
	 * for the subitem and then finding that IRemoteComputer was not 
	 * supported after all. 
	 * http://msdn2.microsoft.com/en-us/library/ms632954.aspx
	 */

	return hr;
}

// EnumObjects() creates a COM object that implements IEnumIDList.
STDMETHODIMP CRemoteFolder::EnumObjects(
	__in_opt HWND hwndOwner, 
	__in SHCONTF dwFlags,
	__deref_out_opt LPENUMIDLIST* ppEnumIDList )
{
	ATLTRACE("CRemoteFolder::EnumObjects called\n");
	ATLASSERT(m_pidl);

	HRESULT hr;

	ATLENSURE( hwndOwner ); // Must be able to display dialog box
    if (ppEnumIDList == NULL)
        return E_POINTER;
    *ppEnumIDList = NULL;

	// Extract connection info from PIDL
	CString strUser, strHost, strPath;
	USHORT uPort;
	ATLASSERT(SUCCEEDED( m_HostPidlManager.IsValid( m_pidl ) ));
	strHost = m_HostPidlManager.GetHost( m_pidl );
	uPort = m_HostPidlManager.GetPort( m_pidl );
	strUser = m_HostPidlManager.GetUser( m_pidl );
	strPath = m_HostPidlManager.GetPath( m_pidl );

    // Create instance of our folder enumerator class
	CComObject<CRemoteEnumIDList>* pEnum;
    hr = CComObject<CRemoteEnumIDList>::CreateInstance( &pEnum );
	ASSERT( SUCCEEDED(hr) );
    if (FAILED(hr))
        return hr;

    // AddRef() the RemoteEnumIDList while we use it
    pEnum->AddRef();

	// Bind this folder to the enumerator.  This will AddRef() the folder
	// to ensure it remains alive as long as the enumerator needs it
	hr = pEnum->BindToFolder( this );

	// Connect enumerator to folder and use connection to populate listing
    if (SUCCEEDED(hr))
		hr = pEnum->ConnectAndFetch( strUser, strHost, strPath, uPort );
    // Return an IEnumIDList interface to the caller.
    if (SUCCEEDED(hr))
        hr = pEnum->QueryInterface( IID_IEnumIDList, (void**)ppEnumIDList );

    pEnum->Release();

    return hr;
}

/*------------------------------------------------------------------------------
 * CRemoteFolder::CreateViewObject : IShellFolder
 * Returns the requested COM interface for aspects of the folder's 
 * functionality, primarily the ShellView object but also context menus etc.
 * Contrasted with GetUIObjectOf which performs a similar function but
 * for the objects containted *within* the folder.
 *----------------------------------------------------------------------------*/
STDMETHODIMP CRemoteFolder::CreateViewObject( __in_opt HWND hwndOwner, 
                                             __in REFIID riid, 
											 __deref_out void **ppvOut )
{
	ATLTRACE("CRemoteFolder::CreateViewObject called\n");
	ATLASSERT(m_pidl); // Check this object has been initialised
	(void)hwndOwner; // Not a custom folder view.  No parent window needed
	
    *ppvOut = NULL;

	HRESULT hr = E_NOINTERFACE;

	if (riid == IID_IShellView)
	{
		ATLTRACE("\t\tRequest: IID_IShellView\n");
		SFV_CREATE sfvData = { sizeof(sfvData), 0 };

		hr = QueryInterface( IID_PPV_ARGS(&sfvData.pshf) );
		if (SUCCEEDED(hr))
		{
			sfvData.psvOuter = NULL;
			sfvData.psfvcb = NULL; 
			hr = SHCreateShellFolderView( &sfvData, (IShellView**)ppvOut );
			sfvData.pshf->Release();
		}
	}
	else if (riid == IID_IShellDetails)
    {
		ATLTRACE("\t\tRequest: IID_IShellDetails\n");
		return QueryInterface(riid, ppvOut);
    }
	
	ATLTRACE("\t\tRequest: <unknown>\n");

    return hr;
}

/*------------------------------------------------------------------------------
 * CRemoteFolder::GetUIObjectOf : IShellFolder
 * Retrieve an optional interface supported by objects in the folder.
 * This method is called when the shell is requesting extra information
 * about an object such as its icon, context menu, thumbnail image etc.
 *----------------------------------------------------------------------------*/
STDMETHODIMP CRemoteFolder::GetUIObjectOf( HWND hwndOwner, UINT cPidl,
	__in_ecount_opt(cPidl) PCUITEMID_CHILD_ARRAY aPidl, REFIID riid,
	__reserved LPUINT puReserved, __out void** ppvReturn )
{
	ATLTRACE("CRemoteFolder::GetUIObjectOf called\n");
	(void)hwndOwner; // No user input required
	(void)puReserved;

	*ppvReturn = NULL;
	
	/*
	IContextMenu    The cidl parameter can be greater than or equal to one.
	IContextMenu2   The cidl parameter can be greater than or equal to one.
	IDataObject     The cidl parameter can be greater than or equal to one.
	IDropTarget     The cidl parameter can only be one.
	IExtractIcon    The cidl parameter can only be one.
	IQueryInfo      The cidl parameter can only be one.
	*/

	if (riid == IID_IExtractIconW)
    {
		ATLTRACE("\t\tRequest: IID_IExtractIconW\n");
		ATLASSERT( cPidl == 1 ); // Only one file 'selected'

		return QueryInterface(riid, ppvReturn);
    }
	else if (riid == IID_IQueryAssociations)
	{
		ATLTRACE("\t\tRequest: IID_IQueryAssociations\n");
		ATLASSERT( cPidl == 1 ); // Only one file 'selected'

		HRESULT hr = AssocCreate( CLSID_QueryAssociations, 
			                      IID_IQueryAssociations,
						          ppvReturn);
		if (SUCCEEDED(hr)) 
		{
			// Get CLSID in {DWORD-WORD-WORD-WORD-WORD.DWORD} form
			LPOLESTR psz;
			StringFromCLSID(__uuidof(CRemoteFolder), &psz);

			// Initialise default assoc provider to use Swish CLSID key for data
			reinterpret_cast<IQueryAssociations*>(*ppvReturn)->Init(
				ASSOCF_INIT_DEFAULTTOSTAR, psz, NULL, NULL);

			CoTaskMemFree( psz );
		}
		return hr;
	}
	
	ATLTRACE("\t\tRequest: <unknown>\n");

    return E_NOINTERFACE;
}

/*------------------------------------------------------------------------------
 * CRemoteFolder::GetDisplayNameOf : IShellFolder
 * Retrieves the display name for the specified file object or subfolder.
 *----------------------------------------------------------------------------*/
STDMETHODIMP CRemoteFolder::GetDisplayNameOf( __in PCUITEMID_CHILD pidl, 
											 __in SHGDNF uFlags, 
											 __out STRRET *pName )
{
	ATLTRACE("CRemoteFolder::GetDisplayNameOf called\n");

	CString strName;

	if (uFlags & SHGDN_FORPARSING)
	{
		// We do not care if the name is relative to the folder or the
		// desktop for the parsing name - always return canonical string:
		//     sftp://username@hostname:port/path

		// TODO:  if !SHGDN_INFOLDER the pidl may not be single-level
		// so we should always seek to the last pidl before extracting info

		strName = _GetLongNameFromPIDL(pidl, true);
	}
	else if(uFlags & SHGDN_FORADDRESSBAR)
	{
		// We do not care if the name is relative to the folder or the
		// desktop for the parsing name - always return canonical string:
		//     sftp://username@hostname:port/path
		// unless the port is the default port in which case it is ommitted:
		//     sftp://username@hostname/path

		strName = _GetLongNameFromPIDL(pidl, false);
	}
	else
	{
		// We do not care if the name is relative to the folder or the
		// desktop for the parsing name - always return the label:
		ATLASSERT(uFlags == SHGDN_NORMAL || uFlags == SHGDN_INFOLDER ||
			(uFlags & SHGDN_FOREDITING));

		strName = _GetPathFromPIDL(pidl);
	}

	// Store in a STRRET and return
	pName->uType = STRRET_WSTR;

	return SHStrDupW( strName, &pName->pOleStr );
}

/*------------------------------------------------------------------------------
 * CRemoteFolder::GetAttributesOf : IShellFolder
 * Returns the attributes for the items whose PIDLs are passed in.
 *----------------------------------------------------------------------------*/
STDMETHODIMP CRemoteFolder::GetAttributesOf(
	UINT cIdl,
	__in_ecount_opt( cIdl ) PCUITEMID_CHILD_ARRAY aPidl,
	__inout SFGAOF *pdwAttribs )
{
	ATLTRACE("CRemoteFolder::GetAttributesOf called\n");
	(void)aPidl; // All items are folders. No need to check PIDL.
	(void)cIdl;

	DWORD dwAttribs = 0;
    dwAttribs |= SFGAO_FOLDER;
    dwAttribs |= SFGAO_HASSUBFOLDER;
    *pdwAttribs &= dwAttribs;

    return S_OK;
}

/*------------------------------------------------------------------------------
 * CRemoteFolder::CompareIDs : IShellFolder
 * Determines the relative order of two file objects or folders.
 * Given their item identifier lists, the two objects are compared and a
 * result code is returned.
 *   Negative: pidl1 < pidl2
 *   Positive: pidl1 > pidl2
 *   Zero:     pidl1 == pidl2
 *----------------------------------------------------------------------------*/
STDMETHODIMP CRemoteFolder::CompareIDs( LPARAM lParam, LPCITEMIDLIST pidl1,
									   LPCITEMIDLIST pidl2 )
{
	ATLTRACE("CRemoteFolder::CompareIDs called\n");
	(void)lParam; // Use default sorting rule always

	ATLASSERT(pidl1 != NULL); ATLASSERT(pidl2 != NULL);
	ATLASSERT(m_PidlManager.GetPath(pidl1).GetLength() > 0 );
	ATLASSERT(m_PidlManager.GetPath(pidl2).GetLength() > 0 );

	// Sort by filename
	return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 
		(unsigned short)wcscmp(m_PidlManager.GetPath(pidl1), 
		                       m_PidlManager.GetPath(pidl2)));
}

/*------------------------------------------------------------------------------
 * CRemoteFolder::EnumSearches : IShellFolder2
 * Returns pointer to interface allowing client to enumerate search objects.
 * We do not support search objects.
 *----------------------------------------------------------------------------*/
STDMETHODIMP CRemoteFolder::EnumSearches( IEnumExtraSearch **ppEnum )
{
	ATLTRACE("CRemoteFolder::EnumSearches called\n");
	(void)ppEnum;
	return E_NOINTERFACE;
}

/*------------------------------------------------------------------------------
 * CRemoteFolder::GetDefaultColumn : IShellFolder2
 * Gets the default sorting and display columns.
 *----------------------------------------------------------------------------*/
STDMETHODIMP CRemoteFolder::GetDefaultColumn( DWORD dwReserved, 
											 __out ULONG *pSort, 
											 __out ULONG *pDisplay )
{
	ATLTRACE("CRemoteFolder::GetDefaultColumn called\n");
	(void)dwReserved;

	// Sort and display by the filename
	*pSort = 0;
	*pDisplay = 0;

	return S_OK;
}

/*------------------------------------------------------------------------------
 * CRemoteFolder::GetDefaultColumnState : IShellFolder2
 * Returns the default state for the column specified by index.
 *----------------------------------------------------------------------------*/
STDMETHODIMP CRemoteFolder::GetDefaultColumnState( __in UINT iColumn, 
												  __out SHCOLSTATEF *pcsFlags )
{
	ATLTRACE("CRemoteFolder::GetDefaultColumnState called\n");

	switch (iColumn)
	{
	case 0: // Display name (Label)
	case 1: // Hostname
	case 2: // Username
	case 4: // Remote filesystem path
		*pcsFlags = SHCOLSTATE_TYPE_STR | SHCOLSTATE_ONBYDEFAULT; break;
	case 3: // SFTP port
		*pcsFlags = SHCOLSTATE_TYPE_INT | SHCOLSTATE_ONBYDEFAULT; break;
	case 5: // Type
		*pcsFlags = SHCOLSTATE_TYPE_STR | SHCOLSTATE_SECONDARYUI; break;
	default:
		return E_FAIL;
	}

	return S_OK;
}

STDMETHODIMP CRemoteFolder::GetDetailsEx( __in PCUITEMID_CHILD pidl, 
										 __in const SHCOLUMNID *pscid,
										 __out VARIANT *pv )
{
	ATLTRACE("CRemoteFolder::GetDetailsEx called\n");

	// If pidl: Request is for an item detail.  Retrieve from pidl and
	//          return string
	// Else:    Request is for a column heading.  Return heading as BSTR

	// Display name (Label)
	if (IsEqualPropertyKey(*pscid, PKEY_ItemNameDisplay))
	{
		ATLTRACE("\t\tRequest: PKEY_ItemNameDisplay\n");
		
		return pidl ?
			_FillDetailsVariant( m_PidlManager.GetPath(pidl), pv ) :
			_FillDetailsVariant( _T("Name"), pv );
	}
	// Owner
	else if (IsEqualPropertyKey(*pscid, PKEY_FileOwner))
	{
		ATLTRACE("\t\tRequest: PKEY_FileOwner\n");
		
		return pidl ?
			_FillDetailsVariant( m_PidlManager.GetOwner(pidl), pv ) :
			_FillDetailsVariant( _T("Owner"), pv );
	}
	// Group
	else if (IsEqualPropertyKey(*pscid, PKEY_SwishRemoteGroup))
	{
		ATLTRACE("\t\tRequest: PKEY_SwishRemoteGroup\n");
		
		return pidl ?
			_FillDetailsVariant( m_PidlManager.GetGroup(pidl), pv ) :
			_FillDetailsVariant( _T("Group"), pv );
	}
	// File permissions: drwxr-xr-x form
	else if (IsEqualPropertyKey(*pscid, PKEY_SwishRemotePermissions))
	{
		ATLTRACE("\t\tRequest: PKEY_SwishRemotePermissions\n");
		
		return pidl ?
			_FillDetailsVariant( m_PidlManager.GetPermissionsStr(pidl), pv ) :
			_FillDetailsVariant( _T("Permissions"), pv );
	}
	// File size in bytes
	else if (IsEqualPropertyKey(*pscid, PKEY_Size))
	{
		ATLTRACE("\t\tRequest: PKEY_Size\n");
		
		return pidl ?
			_FillUI8Variant( m_PidlManager.GetFileSize(pidl), pv ) :
			_FillDetailsVariant( _T("Size"), pv );
	}
	// Last modified date
	else if (IsEqualPropertyKey(*pscid, PKEY_DateModified))
	{
		ATLTRACE("\t\tRequest: PKEY_DateModified\n");

		return pidl ?
			_FillDateVariant( m_PidlManager.GetLastModified(pidl), pv ) :
			_FillDetailsVariant( _T("Last Modified"), pv );
	}
	ATLTRACE("\t\tRequest: <unknown>\n");

	// Assert unless request is one of the supported properties
	// TODO: System.FindData tiggers this
	// UNREACHABLE;

	return E_FAIL;
}

/*------------------------------------------------------------------------------
 * CRemoteFolder::GetDefaultColumnState : IShellFolder2
 * Convert column to appropriate property set ID (FMTID) and property ID (PID).
 * IMPORTANT:  This function defines which details are supported as
 * GetDetailsOf() just forwards the columnID here.  The first column that we
 * return E_FAIL for marks the end of the supported details.
 *----------------------------------------------------------------------------*/
STDMETHODIMP CRemoteFolder::MapColumnToSCID( __in UINT iColumn, 
										    __out PROPERTYKEY *pscid )
{
	ATLTRACE("CRemoteFolder::MapColumnToSCID called\n");

	switch (iColumn)
	{
	case 0: // Display name (Label)
		*pscid = PKEY_ItemNameDisplay; break;
	case 1: // Owner
		*pscid = PKEY_FileOwner; break;
	case 2: // Group
		*pscid = PKEY_SwishRemoteGroup; break;
	case 3: // File Permissions: drwxr-xr-x form
		*pscid= PKEY_SwishRemotePermissions; break;
	case 4: // File size in bytes
		*pscid = PKEY_Size; break;
	case 5: // Last modified date
		*pscid = PKEY_DateModified; break;
	default:
		return E_FAIL;
	}

	return S_OK;
}

/*------------------------------------------------------------------------------
 * CRemoteFolder::CompareIDs : IExtractIcon
 * Extract an icon bitmap given the information passed.
 * We return S_FALSE to tell the shell to extract the icons itself.
 *----------------------------------------------------------------------------*/
STDMETHODIMP CRemoteFolder::Extract( LPCTSTR, UINT, HICON *, HICON *, UINT )
{
	ATLTRACE("CRemoteFolder::Extract called\n");
	return S_FALSE;
}

/*------------------------------------------------------------------------------
 * CRemoteFolder::CompareIDs : IExtractIcon
 * Retrieve the location of the appropriate icon.
 * We set all SFTP hosts to have the icon from shell32.dll.
 *----------------------------------------------------------------------------*/
STDMETHODIMP CRemoteFolder::GetIconLocation(
	__in UINT uFlags, __out_ecount(cchMax) LPTSTR szIconFile, 
	__in UINT cchMax, __out int *piIndex, __out UINT *pwFlags )
{
	ATLTRACE("CRemoteFolder::GetIconLocation called\n");
	(void)uFlags; // type of use is ignored for host folder

	// Set host to have the ICS host icon
	StringCchCopy(szIconFile, cchMax, _T("C:\\WINDOWS\\system32\\shell32.dll"));
	*piIndex = 17;
	*pwFlags = GIL_DONTCACHE;

	return S_OK;
}

/*------------------------------------------------------------------------------
 * CRemoteFolder::GetDetailsOf : IShellDetails
 * Returns detailed information on the items in a folder.
 * This function operates in two distinctly different ways:
 * If pidl is NULL:
 *     Retrieves the information on the view columns, i.e., the names of
 *     the columns themselves.  The index of the desired column is given
 *     in iColumn.  If this column does not exist we return E_FAIL.
 * If pidl is not NULL:
 *     Retrieves the specific item information for the given pidl and the
 *     requested column.
 * The information is returned in the SHELLDETAILS structure.
 *
 * Most of the work is delegated to GetDetailsEx by converting the column
 * index to a PKEY with MapColumnToSCID.  This function also now determines
 * what the index of the last supported detail is.
 *----------------------------------------------------------------------------*/
STDMETHODIMP CRemoteFolder::GetDetailsOf( __in_opt PCUITEMID_CHILD pidl, 
										 __in UINT iColumn, 
										 __out LPSHELLDETAILS pDetails )
{
	ATLTRACE("CRemoteFolder::GetDetailsOf called, iColumn=%u\n", iColumn);

	PROPERTYKEY pkey;

	// Lookup PKEY and use it to call GetDetailsEx
	HRESULT hr = MapColumnToSCID(iColumn, &pkey);
	if (SUCCEEDED(hr))
	{
		VARIANT pv;

		// Get details and convert VARIANT result to SHELLDETAILS for return
		hr = GetDetailsEx(pidl, &pkey, &pv);
		if (SUCCEEDED(hr))
		{
			CString strSrc;

			switch (pv.vt)
			{
			case VT_BSTR:
				strSrc = pv.bstrVal;
				::SysFreeString(pv.bstrVal);

				if(!pidl) // Header requested
					pDetails->fmt = LVCFMT_LEFT;
				break;
			case VT_UI8:
				strSrc.Format(_T("%u"), pv.ullVal);

				if(!pidl) // Header requested
					pDetails->fmt = LVCFMT_RIGHT;
				break;
			case VT_DATE:
				{
				COleDateTime date = pv.date;
				strSrc = date.Format();
				if(!pidl) // Header requested
					pDetails->fmt = LVCFMT_LEFT;
				break;
				}
			default:
				UNREACHABLE;
			}

			pDetails->str.uType = STRRET_WSTR;
			SHStrDup(strSrc, &pDetails->str.pOleStr);

			if(!pidl) // Header requested
				pDetails->cxChar = strSrc.GetLength()^2;
		}

		VariantClear( &pv );
	}

	return hr;
}

STDMETHODIMP CRemoteFolder::ColumnClick( UINT iColumn )
{
	ATLTRACE("CRemoteFolder::ColumnClick called\n");
	(void)iColumn;
	return S_FALSE;
}

/*----------------------------------------------------------------------------*/
/* --- Private functions -----------------------------------------------------*/
/*----------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
 * CRemoteFolder::_GetLongNameFromPIDL
 * Retrieve the long name of the host connection from the given PIDL.
 * The long name is either the canonical form if fCanonical is set:
 *     sftp://username@hostname:port/path
 * or, if not set and if the port is the default port the reduced form:
 *     sftp://username@hostname/path
 *----------------------------------------------------------------------------*/
CString CRemoteFolder::_GetLongNameFromPIDL( PCUITEMID_CHILD pidl, 
										   BOOL fCanonical )
{
	ATLTRACE("CRemoteFolder::_GetLongNameFromPIDL called\n");

	CString strName;
	ATLASSERT(SUCCEEDED(m_PidlManager.IsValid(pidl)));
/*
	// Construct string from info in PIDL
	strName = _T("sftp://");
	strName += m_PidlManager.GetUser(pidl);
	strName += _T("@");
	strName += m_PidlManager.GetHost(pidl);
	if (fCanonical || (m_PidlManager.GetPort(pidl) != SFTP_DEFAULT_PORT))
	{
		strName += _T(":");
		strName.AppendFormat( _T("%u"), m_PidlManager.GetPort(pidl) );
	}
	strName += _T("/");
	strName += m_PidlManager.GetPath(pidl);

	ATLASSERT( strName.GetLength() <= MAX_CANONICAL_LEN );
*/
	return strName;
}

/*------------------------------------------------------------------------------
 * CRemoteFolder::_GetPathFromPIDL
 * Retrieve the file path from the given PIDL.
 *----------------------------------------------------------------------------*/
CString CRemoteFolder::_GetPathFromPIDL( PCUITEMID_CHILD pidl )
{
	ATLTRACE("CRemoteFolder::_GetPathFromPIDL called\n");

	CString strName;
	ATLASSERT(SUCCEEDED(m_PidlManager.IsValid(pidl)));

	// Construct string from info in PIDL
	strName = m_PidlManager.GetPath(pidl);

	ATLASSERT( strName.GetLength() <= MAX_PATH_LEN );

	return strName;
}

/*------------------------------------------------------------------------------
 * CRemoteFolder::_FillDetailsVariant
 * Initialise the VARIANT whose pointer is passed and fill with string data.
 * The string data can be passed in as a wchar array or a CString.  We allocate
 * a new BSTR and store it in the VARIANT.
 *----------------------------------------------------------------------------*/
HRESULT CRemoteFolder::_FillDetailsVariant( __in PCWSTR szDetail,
										   __out VARIANT *pv )
{
	ATLTRACE("CRemoteFolder::_FillDetailsVariant called\n");

	::VariantInit( pv );
	pv->vt = VT_BSTR;
	pv->bstrVal = ::SysAllocString( szDetail );

	return pv->bstrVal ? S_OK : E_OUTOFMEMORY;
}

/*------------------------------------------------------------------------------
 * CRemoteFolder::_FillDateVariant
 * Initialise the VARIANT whose pointer is passed and fill with date info.
 * The date is passed as a CTime.
 *----------------------------------------------------------------------------*/
HRESULT CRemoteFolder::_FillDateVariant( __in CTime dtDate, __out VARIANT *pv )
{
	ATLTRACE("CRemoteFolder::_FillDateVariant called\n");

	::VariantInit( pv );
	pv->vt = VT_DATE;

	// Convert to System Time
	SYSTEMTIME stTemp;
	ATLVERIFY(dtDate.GetAsSystemTime(stTemp));

	// Convert to VARIANT time
	::SystemTimeToVariantTime(&stTemp, &pv->date);
	
	// TODO: There may be a better way to do this than this long chain of
	//       conversions but using COleDateTime is awkward

	return pv->date ? S_OK : E_FAIL;
}

/*------------------------------------------------------------------------------
 * CRemoteFolder::_FillDateVariant
 * Initialise the VARIANT whose pointer is passed and fill with 64-bit unsigned.
 *----------------------------------------------------------------------------*/
HRESULT CRemoteFolder::_FillUI8Variant( __in ULONGLONG ull, __out VARIANT *pv )
{
	ATLTRACE("CRemoteFolder::_FillUI8Variant called\n");

	::VariantInit( pv );
	pv->vt = VT_UI8;
	pv->ullVal = ull;

	return S_OK; // TODO: return success of VariantInit
}

// CRemoteFolder
