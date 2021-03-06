﻿
#include "imcrvtip.h"
#include "TextService.h"
#include "CandidateList.h"
#include "CandidateWindow.h"
#include "InputModeWindow.h"

BOOL CCandidateWindow::_Create(HWND hwndParent, CCandidateWindow *pCandidateWindowParent, DWORD dwUIElementId, UINT depth, BOOL reg, BOOL comp)
{
	_hwndParent = hwndParent;
	_pCandidateWindowParent = pCandidateWindowParent;
	_depth = depth;
	_dwUIElementId = dwUIElementId;

	if(_hwndParent != NULL)
	{
		_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
			CandidateWindowClass, L"", WS_POPUP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			_hwndParent, NULL, g_hInst, this);

		if(_hwnd == NULL)
		{
			return FALSE;
		}

		hFont = _pTextService->hFont;

		if(_pTextService->cx_drawapi && _pTextService->_pD2DFactory != NULL)
		{
			_drawtext_option = _pTextService->_drawtext_option;
			_pD2DFactory = _pTextService->_pD2DFactory;
			_pD2DFactory->AddRef();
			_pD2DDCRT = _pTextService->_pD2DDCRT;
			_pD2DDCRT->AddRef();
			for(int i = 0; i < DISPLAY_COLOR_NUM; i++)
			{
				_pD2DBrush[i] = _pTextService->_pD2DBrush[i];
				_pD2DBrush[i]->AddRef();
			}
			_pDWFactory = _pTextService->_pDWFactory;
			_pDWFactory->AddRef();
			_pDWTF = _pTextService->_pDWTF;
			_pDWTF->AddRef();
		}
	}

	if(_hwnd != NULL && _pTextService->_ShowInputMode)
	{
		try
		{
			_pInputModeWindow = new CInputModeWindow();
			if(!_pInputModeWindow->_Create(_pTextService, NULL, TRUE, _hwnd))
			{
				_pInputModeWindow->_Destroy();
				SafeRelease(&_pInputModeWindow);
			}
		}
		catch(...)
		{
		}
	}

	_reg = reg;
	_comp = comp;

	candidates = _pTextService->candidates;
	candidx = _pTextService->candidx;
	searchkey = _pTextService->searchkey;

	if(reg)
	{
		//辞書登録開始
		if(_hwnd == NULL)
		{
			regwordul = TRUE;
		}
		regword = TRUE;
		regwordtext.clear();
		regwordtextpos = 0;
		comptext.clear();
		regwordfixed = TRUE;

		_BackUpStatus();
		_ClearStatus();
	}

	return TRUE;
}

BOOL CCandidateWindow::_InitClass()
{
	WNDCLASSEXW wcex;

	ZeroMemory(&wcex, sizeof(wcex));
	wcex.cbSize = sizeof(wcex);
	wcex.style = CS_VREDRAW | CS_HREDRAW;
	wcex.lpfnWndProc = CCandidateWindow::_WindowPreProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = g_hInst;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursorW(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = CandidateWindowClass;
	wcex.hIconSm = NULL;

	ATOM atom = RegisterClassExW(&wcex);

	return (atom != 0);
}

void CCandidateWindow::_UninitClass()
{
	UnregisterClassW(CandidateWindowClass, g_hInst);
}

LRESULT CALLBACK CCandidateWindow::_WindowPreProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CCandidateWindow *pCandidateWindow = NULL;

	switch(uMsg)
	{
	case WM_NCCREATE:
		pCandidateWindow = (CCandidateWindow *)((LPCREATESTRUCTW)lParam)->lpCreateParams;
		SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)pCandidateWindow);
		break;
	default:
		pCandidateWindow = (CCandidateWindow *)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
		break;
	}

	if(pCandidateWindow != NULL)
	{
		return pCandidateWindow->_WindowProc(hWnd, uMsg, wParam, lParam);
	}

	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK CCandidateWindow::_WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_PAINT:
		_WindowProcPaint(hWnd);
		break;
	case WM_ERASEBKGND:
		break;
	case WM_MOUSEACTIVATE:
		return MA_NOACTIVATE;
	default:
		return DefWindowProcW(hWnd, uMsg, wParam, lParam);
	}

	return 0;
}

void CCandidateWindow::_Destroy()
{
	if(_hwnd != NULL)
	{
		DestroyWindow(_hwnd);
		_hwnd = NULL;
	}

	if(_pInputModeWindow != NULL)
	{
		_pInputModeWindow->_Destroy();
	}
	SafeRelease(&_pInputModeWindow);

	SafeRelease(&_pDWTF);
	SafeRelease(&_pDWFactory);
	for(int i = 0; i < DISPLAY_COLOR_NUM; i++)
	{
		SafeRelease(&_pD2DBrush[i]);
	}
	SafeRelease(&_pD2DDCRT);
	SafeRelease(&_pD2DFactory);
}

void CCandidateWindow::_Move(LPCRECT lpr, TfEditCookie ec, ITfContext *pContext)
{
	if(_hwnd != NULL && lpr != NULL)
	{
		_rect = *lpr;

		//ignore abnormal position (from CUAS ?)
		if((_rect.top == _rect.bottom) && ((_rect.right - _rect.left) == 1))
		{
			return;
		}

		if(ec != TF_INVALID_EDIT_COOKIE && pContext != NULL)
		{
			_vertical = _pTextService->_GetVertical(ec, pContext);
		}

		if(_vertical)
		{
			LONG w = _rect.right - _rect.left;
			_rect.right += w;
			_rect.left += w;
			_rect.bottom = _rect.top;
		}

		_CalcWindowRect();

		if(_pCandidateWindow != NULL)
		{
#ifdef _DEBUG
			RECT rc;
			GetClientRect(_hwnd, &rc);
			rc.left = _rect.left;
			rc.top += _rect.bottom;
			rc.right = _rect.right;
			rc.bottom += _rect.bottom;
			_pCandidateWindow->_Move(&rc);
#else
			_pCandidateWindow->_Move(&_rect);
#endif
		}
	}
}

void CCandidateWindow::_BeginUIElement()
{
	BOOL bShow = TRUE;

	if(!_reg)
	{
		_InitList();
	}

	_Update();

	if((_hwnd == NULL) && (_depth == 0))
	{
		ITfUIElementMgr *pUIElementMgr;
		if(_pTextService->_GetThreadMgr()->QueryInterface(IID_PPV_ARGS(&pUIElementMgr)) == S_OK)
		{
			pUIElementMgr->BeginUIElement(this, &bShow, &_dwUIElementId);
			if(!bShow)
			{
				pUIElementMgr->UpdateUIElement(_dwUIElementId);
			}
			SafeRelease(&pUIElementMgr);
		}
	}

	if(_hwnd == NULL)
	{
		_bShow = FALSE;
	}
	else
	{
		_bShow = bShow;
	}

	if(_bShow)
	{
		if(_hwnd != NULL)
		{
			SetWindowPos(_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
				SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);

			if(_reg)
			{
				if(_pInputModeWindow != NULL)
				{
					_pInputModeWindow->_Show(TRUE);
				}
			}

			if(_depth == 0)
			{
				NotifyWinEvent(EVENT_OBJECT_IME_SHOW, _hwnd, OBJID_CLIENT, CHILDID_SELF);
			}
		}
	}
}

void CCandidateWindow::_EndUIElement()
{
	if((_hwnd == NULL) && (_depth == 0))
	{
		ITfUIElementMgr *pUIElementMgr;
		if(_pTextService->_GetThreadMgr()->QueryInterface(IID_PPV_ARGS(&pUIElementMgr)) == S_OK)
		{
			pUIElementMgr->EndUIElement(_dwUIElementId);
			SafeRelease(&pUIElementMgr);
		}
	}

	if(_hwnd != NULL)
	{
		SetWindowPos(_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
			SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE | SWP_HIDEWINDOW);

		if(_pInputModeWindow != NULL)
		{
			_pInputModeWindow->_Show(FALSE);
		}

		if(_depth == 0)
		{
			NotifyWinEvent(EVENT_OBJECT_IME_HIDE, _hwnd, OBJID_CLIENT, CHILDID_SELF);
		}
	}

	_bShow = FALSE;
}

BOOL CCandidateWindow::_CanShowUIElement()
{
	BOOL bShow = TRUE;

	ITfUIElementMgr *pUIElementMgr;
	if(_pTextService->_GetThreadMgr()->QueryInterface(IID_PPV_ARGS(&pUIElementMgr)) == S_OK)
	{
		pUIElementMgr->BeginUIElement(this, &bShow, &_dwUIElementId);
		pUIElementMgr->EndUIElement(_dwUIElementId);
		SafeRelease(&pUIElementMgr);
	}

	return bShow;
}

void CCandidateWindow::_Redraw()
{
	if(_hwnd != NULL)
	{
		InvalidateRect(_hwnd, NULL, FALSE);
		UpdateWindow(_hwnd);

		if(_pInputModeWindow != NULL)
		{
			_pInputModeWindow->_Redraw();
		}
	}
}

void CCandidateWindow::_SetText(const std::wstring &text, BOOL fixed, BOOL showcandlist, BOOL showreg)
{
	//CTextService -> CCandidateList -> CCandidateWindow で入力文字列をもらう

	if(_pCandidateWindow != NULL && !_preEnd)
	{
		_pCandidateWindow->_SetText(text, fixed, showcandlist, showreg);
		return;
	}

	if(showreg)
	{
		_CreateNext(TRUE);
	}

	if(showcandlist)
	{
		_CreateNext(FALSE);
	}

	regwordfixed = fixed;

	if(fixed)
	{
		comptext.clear();
		regwordtext.insert(regwordtextpos, text);
		regwordtextpos += text.size();
	}
	else
	{
		comptext = text;
		if(comptext.empty())
		{
			regwordfixed = TRUE;
		}
	}

	_Update();
}

void CCandidateWindow::_GetPrecedingText(std::wstring *text)
{
	if(_pCandidateWindow != NULL && !_preEnd)
	{
		_pCandidateWindow->_GetPrecedingText(text);
		return;
	}

	text->clear();
	text->append(regwordtext.substr(0, regwordtextpos));
}

void CCandidateWindow::_DeletePrecedingText(int delete_count)
{
	if(_pCandidateWindow != NULL && !_preEnd)
	{
		_pCandidateWindow->_DeletePrecedingText(delete_count);
		return;
	}

	if(regwordtextpos - delete_count >= 0 && regwordtext.size() - delete_count >= 0)
	{
		regwordtext.erase(regwordtextpos - delete_count, delete_count);
		regwordtextpos -= delete_count;
	}
	else
	{
		regwordtextpos = 0;
		regwordtext.clear();
	}
}

void CCandidateWindow::_PreEnd()
{
	_preEnd = TRUE;
}

void CCandidateWindow::_End()
{
	_preEnd = FALSE;

#ifndef _DEBUG
	if(_hwnd != NULL)
	{
		SetWindowPos(_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
			SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);
	}
	if(_pInputModeWindow != NULL)
	{
		_pInputModeWindow->_Show(TRUE);
	}
#endif

	if(_pCandidateWindow != NULL)
	{
		_pCandidateWindow->_Destroy();
	}
	SafeRelease(&_pCandidateWindow);

	if(_hwnd == NULL)
	{
		_dwFlags = TF_CLUIE_DOCUMENTMGR | TF_CLUIE_COUNT | TF_CLUIE_SELECTION |
			TF_CLUIE_STRING | TF_CLUIE_PAGEINDEX | TF_CLUIE_CURRENTPAGE;
		_Update();
		_UpdateUIElement();
	}
}

void CCandidateWindow::_UpdateComp()
{
	_comp = TRUE;
	candidates = _pTextService->candidates;
	candidx = _pTextService->candidx;
	searchkey = _pTextService->searchkey;

	_InitList();
	_Update();
	_UpdateUIElement();
}

void CCandidateWindow::_InitList()
{
	UINT i;

	if(!_comp)
	{
		_uPageCandNum = MAX_SELKEY;
	}
	else
	{
		_uPageCandNum = _pTextService->cx_compmultinum;
		if(_uPageCandNum > MAX_SELKEY_C || _uPageCandNum < 1)
		{
			_uPageCandNum = MAX_SELKEY;
		}
	}

	if(!_comp)
	{
		_uShowedCount = _pTextService->cx_untilcandlist - 1;
	}
	else
	{
		_uShowedCount = 0;
	}
	_uCount = (UINT)candidates.size() - _uShowedCount;

	_CandStr.clear();
	for(i = 0; i < _uCount; i++)
	{
		if(!_comp)
		{
			_CandStr.push_back(_pTextService->selkey[(i % _uPageCandNum)][0]);
			_CandStr[i].append(markNo);
		}
		else
		{
			_CandStr.push_back(L"");
		}

		_CandStr[i].append(candidates[_uShowedCount + i].first.first);

		if(_pTextService->cx_annotation &&
			!candidates[_uShowedCount + i].first.second.empty())
		{
			if(!_comp)
			{
				_CandStr[i].append(markAnnotation);
			}
			else
			{
				_CandStr[i].append(markSP);
			}
			_CandStr[i].append(candidates[_uShowedCount + i].first.second);
		}
	}

	_uPageCnt = ((_uCount - (_uCount % _uPageCandNum)) / _uPageCandNum) + ((_uCount % _uPageCandNum) == 0 ? 0 : 1);

	_PageIndex.clear();
	_CandCount.clear();
	for(i = 0; i < _uPageCnt; i++)
	{
		_PageIndex.push_back(i * _uPageCandNum);
		_CandCount.push_back((i < (_uPageCnt - 1)) ? _uPageCandNum :
			(((_uCount % _uPageCandNum) == 0) ? _uPageCandNum : (_uCount % _uPageCandNum)));
	}

	_uIndex = 0;

	_dwFlags = TF_CLUIE_DOCUMENTMGR | TF_CLUIE_COUNT | TF_CLUIE_SELECTION |
		TF_CLUIE_STRING | TF_CLUIE_PAGEINDEX | TF_CLUIE_CURRENTPAGE;
}

void CCandidateWindow::_UpdateUIElement()
{
	if(!_bShow)
	{
		ITfUIElementMgr *pUIElementMgr;
		if(_pTextService->_GetThreadMgr()->QueryInterface(IID_PPV_ARGS(&pUIElementMgr)) == S_OK)
		{
			pUIElementMgr->UpdateUIElement(_dwUIElementId);
			SafeRelease(&pUIElementMgr);
		}
	}
}

void CCandidateWindow::_NextPage()
{
	UINT uOldPage, uNewPage;

	GetCurrentPage(&uOldPage);
	uNewPage = uOldPage + 1;
	if(uNewPage >= _uPageCnt)
	{
		if(_pCandidateList != NULL)
		{
			if(_hwnd == NULL)
			{
				regwordul = TRUE;
			}

			if(!regword)
			{
				//辞書登録開始
				regword = TRUE;
				regwordtext.clear();
				regwordtextpos = 0;
				comptext.clear();
				regwordfixed = TRUE;

				_BackUpStatus();
				_ClearStatus();

				if(_pInputModeWindow != NULL)
				{
					_pInputModeWindow->_Show(TRUE);
				}
			}
			else
			{
				_CreateNext(TRUE);
			}

			_Update();
			return;
		}
	}

	_uIndex = _PageIndex[uNewPage];

	_dwFlags = TF_CLUIE_SELECTION;
	if(uNewPage != uOldPage)
	{
		_dwFlags |= TF_CLUIE_CURRENTPAGE;
	}

	_Update();
	_UpdateUIElement();
}

void CCandidateWindow::_PrevPage()
{
	UINT uOldPage, uNewPage;

	GetCurrentPage(&uOldPage);
	if(uOldPage > 0)
	{
		uNewPage = uOldPage - 1;
	}
	else
	{
		if(_pCandidateList != NULL)
		{
			if(!regword)
			{
				if(_pTextService->cx_untilcandlist == 1)
				{
					if(_pCandidateWindowParent == NULL)
					{
						_InvokeSfHandler(SKK_CANCEL);
					}
					else
					{
						if(_reg)
						{
							_RestoreStatusReg();
						}
						_PreEndReq();
						_HandleKey(0, SKK_CANCEL);
						_EndReq();
					}
				}
				else
				{
					if(_pCandidateWindowParent == NULL)
					{
						_pTextService->candidx = _pTextService->cx_untilcandlist - 1;
						_InvokeSfHandler(SKK_PREV_CAND);
					}
					else
					{
						if(_reg)
						{
							_RestoreStatusReg();
						}
						_PreEndReq();
						_pTextService->candidx = _pTextService->cx_untilcandlist - 1;
						_HandleKey(0, SKK_PREV_CAND);
						_EndReq();
					}
				}
			}
			else
			{
				if(_pTextService->cx_untilcandlist == 1)
				{
					_HandleKey(0, SKK_CANCEL);
				}
				else
				{
					_pTextService->candidx = _pTextService->cx_untilcandlist - 1;
					_HandleKey(0, SKK_PREV_CAND);
				}

				_Update();
				_UpdateUIElement();
			}
		}
		return;
	}

	_uIndex = _PageIndex[uNewPage];

	_dwFlags = TF_CLUIE_SELECTION;
	if(uNewPage != uOldPage)
	{
		_dwFlags |= TF_CLUIE_CURRENTPAGE;
	}

	_Update();
	_UpdateUIElement();
}

void CCandidateWindow::_NextComp()
{
	UINT uOldPage, uNewPage;

	GetCurrentPage(&uOldPage);

	if(_uIndex + 1 >= _uCount)
	{
		return;
	}

	_InvokeSfHandler(SKK_NEXT_COMP);

	candidx++;

	_uIndex++;
	GetCurrentPage(&uNewPage);

	_dwFlags = TF_CLUIE_SELECTION;
	if(uNewPage != uOldPage)
	{
		_dwFlags |= TF_CLUIE_CURRENTPAGE;
	}

	_Update();
	_UpdateUIElement();
}

void CCandidateWindow::_PrevComp()
{
	UINT uOldPage, uNewPage;

	GetCurrentPage(&uOldPage);

	if(_uIndex == 0)
	{
		if((_pTextService->cx_stacompmulti && !_pTextService->cx_dyncompmulti) ||
			//closed at _DynamicComp
			_pTextService->searchkey.empty())
		{
			_InvokeSfHandler(SKK_PREV_COMP);
			return;
		}
	}

	_InvokeSfHandler(SKK_PREV_COMP);

	if(_uIndex == 0)
	{
		candidx = (size_t)-1;
		_InitList();
		_Update();
		_UpdateUIElement();
		return;
	}

	candidx--;

	_uIndex--;
	GetCurrentPage(&uNewPage);

	_dwFlags = TF_CLUIE_SELECTION;
	if(uNewPage != uOldPage)
	{
		_dwFlags |= TF_CLUIE_CURRENTPAGE;
	}

	_Update();
	_UpdateUIElement();
}

void CCandidateWindow::_Update()
{
	if(regwordul || regword)
	{
		disptext = _MakeRegWordString();
	}

	if(regwordul)
	{
		_dwFlags = TF_CLUIE_COUNT | TF_CLUIE_SELECTION | TF_CLUIE_STRING |
			TF_CLUIE_PAGEINDEX | TF_CLUIE_CURRENTPAGE;
		_UpdateUIElement();
	}
	else
	{
		if(_hwnd != NULL)
		{
			_CalcWindowRect();

			_Redraw();
		}
	}
}

void CCandidateWindow::_BackUpStatus()
{
	inputmode_bak = _pTextService->inputmode;
	abbrevmode_bak = _pTextService->abbrevmode;
	kana_bak = _pTextService->kana;
	okuriidx_bak = _pTextService->okuriidx;
	cursoridx_bak = _pTextService->cursoridx;
	searchkey_bak = _pTextService->searchkey;
	searchkeyorg_bak = _pTextService->searchkeyorg;
	candidates_bak = _pTextService->candidates;
	candidx_bak = _pTextService->candidx;
	candorgcnt_bak = _pTextService->candorgcnt;
}

void CCandidateWindow::_ClearStatus()
{
	//_pTextService->inputmode //そのまま
	_pTextService->abbrevmode = FALSE;
	_pTextService->kana.clear();
	_pTextService->okuriidx = 0;
	_pTextService->cursoridx = 0;
	_pTextService->searchkey.clear();
	_pTextService->searchkeyorg.clear();
	_pTextService->candidates.clear();
	_pTextService->candidx = 0;
	_pTextService->candorgcnt = 0;
	_pTextService->showcandlist = FALSE;
	_pTextService->showentry = FALSE;
	_pTextService->inputkey = FALSE;
}

void CCandidateWindow::_RestoreStatusReg()
{
	_pTextService->inputmode = inputmode_bak;
	_pTextService->_UpdateLanguageBar();
	_pTextService->abbrevmode = abbrevmode_bak;
	_pTextService->kana = kana_bak;
	_pTextService->okuriidx = okuriidx_bak;
	_pTextService->cursoridx = cursoridx_bak;
	_pTextService->searchkey = searchkey_bak;
	_pTextService->searchkeyorg = searchkeyorg_bak;
	_pTextService->candidates = candidates_bak;
	_pTextService->candidx = candidx_bak;
	_pTextService->candorgcnt = candorgcnt_bak;
	_pTextService->showcandlist = TRUE;
	_pTextService->showentry = TRUE;
	_pTextService->inputkey = TRUE;
}

void CCandidateWindow::_ClearStatusReg()
{
	inputmode_bak = im_default;
	abbrevmode_bak = FALSE;
	kana_bak.clear();
	okuriidx_bak = 0;
	cursoridx_bak = 0;
	searchkey_bak.clear();
	searchkeyorg_bak.clear();
	candidates_bak.clear();
	candidx_bak = 0;
	candorgcnt_bak = 0;
}

void CCandidateWindow::_PreEndReq()
{
	if(_pCandidateWindowParent != NULL && !_preEnd)
	{
		_pCandidateWindowParent->_PreEnd();
	}
}

void CCandidateWindow::_EndReq()
{
	if(_pCandidateWindowParent != NULL && !_preEnd)
	{
		_pCandidateWindowParent->_End();
	}
}

void CCandidateWindow::_CreateNext(BOOL reg)
{
	try
	{
		_pCandidateWindow = new CCandidateWindow(_pTextService, _pCandidateList);
		_pCandidateWindow->_Create(_hwndParent, this, _dwUIElementId, _depth + 1, reg, FALSE);

#ifdef _DEBUG
		RECT rc;
		GetClientRect(_hwnd, &rc);
		rc.left = _rect.left;
		rc.top += _rect.bottom;
		rc.right = _rect.right;
		rc.bottom += _rect.bottom;
		_pCandidateWindow->_Move(&rc);
#else
		_pCandidateWindow->_Move(&_rect);
#endif
		_pCandidateWindow->_BeginUIElement();

#ifndef _DEBUG
		if(_hwnd != NULL)
		{
			SetWindowPos(_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
				SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE | SWP_HIDEWINDOW);
		}

		if(_pInputModeWindow != NULL)
		{
			_pInputModeWindow->_Show(FALSE);
		}
#endif
	}
	catch(...)
	{
	}
}
