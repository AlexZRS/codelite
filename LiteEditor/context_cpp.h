//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// copyright            : (C) 2008 by Eran Ifrah                            
// file name            : context_cpp.h              
//                                                                          
// -------------------------------------------------------------------------
// A                                                                        
//              _____           _      _     _ _                            
//             /  __ \         | |    | |   (_) |                           
//             | /  \/ ___   __| | ___| |    _| |_ ___                      
//             | |    / _ \ / _  |/ _ \ |   | | __/ _ )                     
//             | \__/\ (_) | (_| |  __/ |___| | ||  __/                     
//              \____/\___/ \__,_|\___\_____/_|\__\___|                     
//                                                                          
//                                                  F i l e                 
//                                                                          
//    This program is free software; you can redistribute it and/or modify  
//    it under the terms of the GNU General Public License as published by  
//    the Free Software Foundation; either version 2 of the License, or     
//    (at your option) any later version.                                   
//                                                                          
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
 #ifndef CONTEXT_CPP_H
#define CONTEXT_CPP_H

#include "context_base.h"
#include "cl_calltip.h"
#include <map>
#include "entry.h"

class RefactorSource;

class ContextCpp : public ContextBase {
	clCallTipPtr m_ct;
	std::map<wxString, int> m_propertyInt;
	std::vector<wxMenuItem*> m_dynItems;
	wxString m_selectedWord;

	enum TipKind
	{
		TipNone = -1,
		TipHover,
		TipFuncProto
	};

	enum CalltipClickPos
	{
		Elsewhere = 0,
		ArrowUp ,
		ArrowDown
	};

	TipKind m_tipKind;
	wxMenu *m_rclickMenu;

	//images used by the C++ context
	static wxBitmap m_classBmp;
	static wxBitmap m_structBmp;
	static wxBitmap m_namespaceBmp;
	static wxBitmap m_variableBmp;
	static wxBitmap m_tpyedefBmp;
	static wxBitmap m_memberPrivateBmp;
	static wxBitmap m_memberPublicBmp;
	static wxBitmap m_memberProtectedeBmp;
	static wxBitmap m_functionPrivateBmp;
	static wxBitmap m_functionPublicBmp;
	static wxBitmap m_functionProtectedeBmp;
	static wxBitmap m_macroBmp;
	static wxBitmap m_enumBmp;
	static wxBitmap m_enumeratorBmp;
	static wxBitmap m_cppFileBmp;
	static wxBitmap m_hFileBmp;
	static wxBitmap m_otherFileBmp;

private:
	bool TryOpenFile(const wxFileName &fileName);
	void DisplayCompletionBox(const std::vector<TagEntryPtr> &tags, const wxString &word, bool showFullDecl);
	void DisplayFilesCompletionBox(const wxString &word);
	bool DoGetFunctionBody(long curPos, long &blockStartPos, long &blockEndPos, wxString &content);
	void Initialize();
	
public:
	ContextCpp(LEditor *container);
	virtual ~ContextCpp();
	ContextCpp();
	virtual ContextBase *NewInstance(LEditor *container);

	virtual void CompleteWord();
	virtual void CodeComplete();
	virtual void GotoDefinition();
	virtual void GotoPreviousDefintion();
	virtual void AutoIndent(const wxChar&);
	virtual void CallTipCancel();
	virtual	bool IsCommentOrString(long pos);
	virtual	bool IsComment(long pos);
	virtual void AddMenuDynamicContent(wxMenu *menu);
	virtual void RemoveMenuDynamicContent(wxMenu *menu);
	virtual void ApplySettings();
	virtual void RetagFile();
	virtual wxString CallTipContent();
	
	//override swapfiles features
	virtual void SwapFiles(const wxFileName &fileName);

	//Event handlers
	virtual void OnDwellEnd(wxScintillaEvent &event);
	virtual void OnDwellStart(wxScintillaEvent &event);
	virtual void OnDbgDwellEnd(wxScintillaEvent &event);
	virtual void OnDbgDwellStart(wxScintillaEvent &event);
	virtual void OnCallTipClick(wxScintillaEvent &event);
	virtual void OnSciUpdateUI(wxScintillaEvent &event);
	virtual void OnFileSaved();
	virtual void AutoAddComment();
	
	//Capture menu events
	//return this context specific right click menu
	virtual wxMenu *GetMenu(){return m_rclickMenu;}
	virtual void OnSwapFiles(wxCommandEvent &event);
	virtual void OnInsertDoxyComment(wxCommandEvent &event);
	virtual void OnCommentSelection(wxCommandEvent &event);
	virtual void OnCommentLine(wxCommandEvent &event);
	virtual void OnGenerateSettersGetters(wxCommandEvent &event);
	virtual void OnFindImpl(wxCommandEvent &event);
	virtual void OnFindDecl(wxCommandEvent &event);
	virtual void OnKeyDown(wxKeyEvent &event);
	virtual void OnUpdateUI(wxUpdateUIEvent &event);
	virtual void OnContextOpenDocument(wxCommandEvent &event);
	virtual void OnAddIncludeFile(wxCommandEvent &e);
	virtual void OnMoveImpl(wxCommandEvent &e);
	virtual void OnAddImpl(wxCommandEvent &e);
	virtual void OnAddMultiImpl(wxCommandEvent &e);
	virtual void OnRenameFunction(wxCommandEvent &e);
	virtual void OnRetagFile(wxCommandEvent &e);
	virtual void OnUserTypedXChars(const wxString &word);
	
	DECLARE_EVENT_TABLE();
private:
	wxString GetWordUnderCaret();
	wxString GetFileImageString(const wxString &ext);
	wxString GetImageString(const TagEntry &entry);
	wxString GetExpression(long pos, bool onlyWord, LEditor *editor = NULL);
	void DoGotoSymbol(const std::vector<TagEntryPtr> &tags);
	bool IsIncludeStatement(const wxString &line, wxString *fileName = NULL);
	void RemoveDuplicates(std::vector<TagEntryPtr>& src, std::vector<TagEntryPtr>& target);
	void PrependMenuItem(wxMenu* menu, const wxString &text, wxObjectEventFunction func);
	void PrependMenuItem(wxMenu* menu, const wxString &text, int id);
	void PrependMenuItemSeparator(wxMenu* menu);
	int FindLineToAddInclude();
	void MakeCppKeywordsTags(const wxString &word, std::vector<TagEntryPtr> &tags);
	
	
	/**
	 * \brief try to find a swapped file for this rhs. The logic is based on the C++ coding conventions
	 * a swapped file for a.cpp will be a.h or a.hpp
	 * \param rhs input
	 * \param lhs output
	 * \return true if such sibling file exist, false otherwise
	 */
	bool FindSwappedFile(const wxFileName &rhs, wxString &lhs);
	
	/**
	 * \brief 
	 * \param ctrl
	 * \param pos
	 * \param word
	 * \param rs
	 * \return 
	 */
	bool ResolveWord(LEditor *ctrl, int pos, const wxString &word, RefactorSource *rs);
};

#endif // CONTEXT_CPP_H


