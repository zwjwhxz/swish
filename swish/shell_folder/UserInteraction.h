/*  Component to handle user-interaction between the user and an SftpProvider.

    Copyright (C) 2008  Alexander Lamaison <awl03@doc.ic.ac.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

#include "SftpProvider.h"   // ISftpProvider/ISftpConsumer interfaces

#include "common/atl.hpp"          // Common ATL setup

class ATL_NO_VTABLE CUserInteraction :
	public ISftpConsumer,
	public ATL::CComObjectRoot
{
public:

	BEGIN_COM_MAP(CUserInteraction)
		COM_INTERFACE_ENTRY(ISftpConsumer)
	END_COM_MAP()

	CUserInteraction() : m_hwndOwner(NULL) {}

	HRESULT Initialize( __in_opt HWND hwndOwner );

	/**
	 * Create and initialise an instance of the CUserInteraction class.
	 *
	 * @param [in]  hwndOwner  A window handle to parent window which this 
	 *                         instance should use as the parent for any 
	 *                         user-interaction.
	 * @param [out] ppReturn   The location in which to return the 
	 *                         ISftpConsumer interace pointer for this instance.
	 */
	static HRESULT MakeInstance(
		__in_opt HWND hwndOwner, __deref_out ISftpConsumer **ppReturn )
	{
		HRESULT hr;

		ATL::CComObject<CUserInteraction> *pConsumer;
		hr = ATL::CComObject<CUserInteraction>::CreateInstance(&pConsumer);
		ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr );

		pConsumer->AddRef();

		pConsumer->Initialize(hwndOwner);
		hr = pConsumer->QueryInterface(ppReturn);
		ATLASSERT(SUCCEEDED(hr));

		pConsumer->Release();
		pConsumer = NULL;

		return hr;
	}

	/**
	 * User interaction callbacks.
	 * @{
	 */
	// ISftpConsumer Methods
	IFACEMETHODIMP OnPasswordRequest(
		__in BSTR bstrRequest, __out BSTR *pbstrPassword
	);
	IFACEMETHODIMP OnKeyboardInteractiveRequest(
		__in BSTR bstrName, __in BSTR bstrInstruction,
		__in SAFEARRAY *psaPrompts,
		__in SAFEARRAY *psaShowResponses,
		__deref_out SAFEARRAY **ppsaResponses
	);
	IFACEMETHODIMP OnYesNoCancel(
		__in BSTR bstrMessage,
		__in_opt BSTR bstrYesInfo,
		__in_opt BSTR bstrNoInfo,
		__in_opt BSTR bstrCancelInfo,
		__in_opt BSTR bstrTitle,
		__out int *piResult
	);
	IFACEMETHODIMP OnConfirmOverwrite(
		__in BSTR bstrOldFile,
		__in BSTR bstrExistingFile
	);
	IFACEMETHODIMP OnConfirmOverwriteEx(
		__in Listing ltOldFile,
		__in Listing ltExistingFile
	);
	IFACEMETHODIMP OnReportError(
		__in BSTR bstrMessage
	);
	/* @} */

private:
	HWND m_hwndOwner; ///< Window to use as parent for user interaction.
};