#include <wx/textbuf.h>
#include <wx/textfile.h>
#include "virtualdirectoryselector.h"
#include <wx/filefn.h>
#include "project.h"
#include "templateclassdlg.h"
#include <wx/msgdlg.h>
#include "swStringDb.h"
#include "snipwiz.h"
#include "swGlobals.h"
#include "imanager.h"

static const wxString swHeader = wxT( "header" );
static const wxString swSource = wxT( "source" );
static const wxString swPhClass = wxT( "%CLASS%" );

TemplateClassDlg::TemplateClassDlg( wxWindow* parent, SnipWiz *plugin, IManager *manager )
		: TemplateClassBaseDlg( parent )
		, m_plugin(plugin)
		, m_pManager(manager)
{
	Initialize();
}

void TemplateClassDlg::Initialize()
{
	// set tab sizes
	wxTextAttr attribs = m_textCtrlHeader->GetDefaultStyle();
	wxArrayInt	tabs = attribs.GetTabs();
	int tab = 70;
	for ( int i = 1; i < 20; i++ )
		tabs.Add( tab * i );
	attribs.SetTabs( tabs );
	m_textCtrlHeader->SetDefaultStyle( attribs );
	m_textCtrlImpl->SetDefaultStyle( attribs );

	GetStringDb()->Load( m_pluginPath + defaultTmplFile );

	wxArrayString templates;
	GetStringDb()->GetAllSets( templates );
	for ( wxUint32 i = 0; i < templates.GetCount(); i++ ) {
		m_comboxTemplates->AppendString( templates[i] );
		m_comboxCurrentTemplate->AppendString( templates[i] );
	}
	if ( templates.GetCount() ) {
		m_comboxTemplates->Select( 0 );
		wxString set = m_comboxTemplates->GetValue();
		m_textCtrlHeader->SetValue( GetStringDb()->GetString( set, swHeader ) );
		m_textCtrlImpl->SetValue( GetStringDb()->GetString( set, swSource ) );
		m_comboxCurrentTemplate->Select( 0 );
	}
	TreeItemInfo item = m_pManager->GetSelectedTreeItemInfo( TreeFileView );
	if ( item.m_item.IsOk() && item.m_itemType == ProjectItem::TypeVirtualDirectory ) {
		m_virtualFolder = VirtualDirectorySelector::DoGetPath( m_pManager->GetTree( TreeFileView ), item.m_item, false );
		m_projectPath =  item.m_fileName.GetPath( wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR );
	}
	m_textCtrlVD->SetValue( m_virtualFolder );
	m_textCtrlFilePath->SetValue( m_projectPath );
}

void TemplateClassDlg::OnClassNameEntered( wxCommandEvent& event )
{
	wxUnusedVar( event );
	wxString buffer = m_textCtrlClassName->GetValue();
	if ( buffer.IsEmpty() ) {
		m_textCtrlHeaderFile->SetValue(wxT(""));
		m_textCtrlCppFile->SetValue(wxT(""));
		return;
	} else {
		m_textCtrlHeaderFile->SetValue( buffer.Lower() + wxT( ".h" ) );
		m_textCtrlCppFile->SetValue( buffer.Lower() + wxT( ".cpp" ) );
	}
}

void TemplateClassDlg::OnBrowseVD( wxCommandEvent& event )
{
	wxUnusedVar( event );
	VirtualDirectorySelector dlg( this, m_pManager->GetWorkspace(), m_textCtrlVD->GetValue() );
	if ( dlg.ShowModal() == wxID_OK ) {
		m_textCtrlVD->SetValue( dlg.GetVirtualDirectoryPath() );
		m_textCtrlVD->SetForegroundColour(wxColour(0,128,0));
		m_textCtrlVD->Refresh();
	}
}

void TemplateClassDlg::OnBrowseFilePath( wxCommandEvent& event )
{
	wxUnusedVar( event );
	wxString dir = wxT("");
	if ( wxFileName::DirExists( m_projectPath ) ) {
		dir = m_projectPath;
	}

	dir = wxDirSelector( wxT( "Select output folder" ), dir, wxDD_DEFAULT_STYLE, wxDefaultPosition, this);
	if ( !dir.IsEmpty() ) {
		m_projectPath = dir;
		m_textCtrlFilePath->SetValue(m_projectPath);
	}
}

void TemplateClassDlg::OnGenerate( wxCommandEvent& event )
{
	wxUnusedVar(event);
	wxArrayString files;
	wxString newClassName = m_textCtrlClassName->GetValue();
	wxString baseClass = m_comboxCurrentTemplate->GetValue();

	if (!wxEndsWithPathSeparator(m_projectPath))
		m_projectPath += wxFILE_SEP_PATH;

	wxString buffer = GetStringDb()->GetString( baseClass, swHeader );
	buffer.Replace( swPhClass, newClassName );
	files.Add( m_projectPath + m_textCtrlHeaderFile->GetValue() );
	SaveBufferToFile( files.Item(0), buffer );

	buffer = wxString::Format( wxT( "#include \"%s\"%s%s" ), m_textCtrlHeaderFile->GetValue().c_str(), eol[m_curEol].c_str(), eol[m_curEol].c_str() );
	buffer += GetStringDb()->GetString( baseClass, swSource );
	buffer.Replace( swPhClass, newClassName );
	files.Add( m_projectPath + m_textCtrlCppFile->GetValue() );
	SaveBufferToFile( files.Item(1), buffer );

	if ( !m_textCtrlVD->GetValue().IsEmpty() ) {
		m_pManager->AddFilesToVirtualFodler( m_textCtrlVD->GetValue(), files );
	}

	wxString msg;
	msg << wxString::Format( wxT( "%s\n" ), files[0].c_str() )
	<< wxString::Format( wxT( "%s\n\n" ), files[1].c_str() )
	<< wxT( "Files successfully created." );
	wxMessageBox(msg, wxT( "Add template class" ));
	EndModal(wxID_OK);
}

void TemplateClassDlg::OnGenerateUI( wxUpdateUIEvent& event )
{
	wxUnusedVar( event );
	if ( m_comboxCurrentTemplate->GetSelection() == wxNOT_FOUND ||
	        m_textCtrlClassName->GetValue().IsEmpty() ||
	        m_textCtrlHeaderFile->GetValue().IsEmpty() ||
	        m_textCtrlCppFile->GetValue().IsEmpty() )
		event.Enable( false );
	else
		event.Enable( true );
}

void TemplateClassDlg::OnQuit( wxCommandEvent& event )
{
	wxUnusedVar(event);
	GetStringDb()->Save( m_pluginPath + defaultTmplFile );
	EndModal(wxID_CANCEL);
}

void TemplateClassDlg::OnTemplateClassSelected( wxCommandEvent& event )
{
	wxUnusedVar( event );
	wxString set = m_comboxTemplates->GetValue();
	if ( GetStringDb()->IsSet( set ) ) {
		m_textCtrlHeader->SetValue( GetStringDb()->GetString( set, swHeader ) );
		m_textCtrlImpl->SetValue( GetStringDb()->GetString( set, swSource) );
	}
}

void TemplateClassDlg::OnButtonAdd( wxCommandEvent& event )
{
	wxUnusedVar( event );
	wxString set = m_comboxTemplates->GetValue();
	bool isSet = GetStringDb()->IsSet( set );
	if ( isSet ) {
		int ret = wxMessageBox( wxT( "Class exists!\nOverwrite?" ), wxT( "Add class" ), wxYES_NO | wxICON_QUESTION );
		if ( ret == wxNO )
			return;
	}
	GetStringDb()->SetString( set, swHeader, m_textCtrlHeader->GetValue() );
	GetStringDb()->SetString( set, swSource, m_textCtrlImpl->GetValue() );
	if ( !isSet ) {
		m_comboxTemplates->AppendString( set );
	}

	RefreshTemplateList();
	m_modified = true;
}

void TemplateClassDlg::OnButtonAddUI( wxUpdateUIEvent& event )
{
	if ( m_comboxTemplates->GetValue().IsEmpty() || m_textCtrlHeader->GetValue().IsEmpty() || m_textCtrlImpl->GetValue().IsEmpty() )
		event.Enable( false );
	else
		event.Enable( true );
}

void TemplateClassDlg::OnButtonChange( wxCommandEvent& event )
{
	wxUnusedVar( event );
	wxString set = m_comboxTemplates->GetValue();
	bool isSet = GetStringDb()->IsSet( set );
	if ( !isSet ) {
		int ret = ::wxMessageBox( wxT( "Class doesn't exists!\nAdd new?" ), wxT( "Change class" ), wxYES_NO | wxICON_QUESTION );
		if ( ret == wxNO )
			return;
	}
	GetStringDb()->SetString( set, swHeader, m_textCtrlHeader->GetValue() );
	GetStringDb()->SetString( set, swSource, m_textCtrlImpl->GetValue() );
	if ( !isSet )
		m_comboxTemplates->AppendString( set );
	RefreshTemplateList();
	m_modified = true;
}

void TemplateClassDlg::OnButtonChangeUI( wxUpdateUIEvent& event )
{
	if ( m_comboxTemplates->GetSelection() == wxNOT_FOUND )
		event.Enable( false );
	else
		event.Enable( true );
	if ( m_textCtrlHeader->IsModified() == false && m_textCtrlImpl->IsModified() == false )
		event.Enable( false );
}

void TemplateClassDlg::OnButtonRemove( wxCommandEvent& event )
{
	wxUnusedVar( event );
	wxString set = m_comboxTemplates->GetValue();
	bool isSet = GetStringDb()->IsSet( set );
	if ( !isSet ) {
		::wxMessageBox( wxT( "Class not found!\nNothing to remove." ), wxT( "Remove class" ) );
		return;
	}
	GetStringDb()->DeleteKey( set, swHeader );
	GetStringDb()->DeleteKey( set, swSource );
	long index = m_comboxTemplates->FindString( set );
	m_comboxTemplates->Delete( index );
	RefreshTemplateList();
	m_modified = true;
}

void TemplateClassDlg::OnButtonRemoveUI( wxUpdateUIEvent& event )
{
	if ( m_comboxTemplates->GetSelection() == wxNOT_FOUND )
		event.Enable( false );
	else
		event.Enable( true );
}

void TemplateClassDlg::OnButtonClear(wxCommandEvent& e)
{
	m_comboxTemplates->SetValue( wxT( "" ) );
	m_textCtrlImpl->SetValue( wxT( "" ) );
	m_textCtrlHeader->SetValue( wxT( "" ) );
}

void TemplateClassDlg::OnButtonClearUI(wxUpdateUIEvent& e)
{
	e.Enable(true);
}

void TemplateClassDlg::OnInsertClassKeyword( wxCommandEvent& event )
{
	wxUnusedVar( event );
	long from, to;

	if ( m_notebookFiles->GetSelection() == 0 ) {
		m_textCtrlHeader->GetSelection( &from, &to );
		m_textCtrlHeader->Replace( from, to, swPhClass );
		m_textCtrlHeader->SetFocus();
	} else {
		m_textCtrlImpl->GetSelection( &from, &to );
		m_textCtrlImpl->Replace( from, to, swPhClass );
		m_textCtrlImpl->SetFocus();
	}
}

void TemplateClassDlg::OnInsertClassKeywordUI( wxUpdateUIEvent& event )
{
	event.Enable(true);
}

void TemplateClassDlg::OnHeaderFileContentChnaged( wxCommandEvent& event )
{
	event.Skip();
}

void TemplateClassDlg::OnImplFileContentChnaged( wxCommandEvent& event )
{
	event.Skip();
}

swStringDb* TemplateClassDlg::GetStringDb()
{
	return m_plugin->GetStringDb();
}

void TemplateClassDlg::RefreshTemplateList()
{
	wxArrayString templates;
	GetStringDb()->GetAllSets( templates );
	m_comboxCurrentTemplate->Clear();

	for ( wxUint32 i = 0; i < templates.GetCount(); i++ ) {
		m_comboxCurrentTemplate->AppendString( templates[i] );
	}

	if ( templates.GetCount() ) {
		m_comboxCurrentTemplate->Select( 0 );
	}
}

bool TemplateClassDlg::SaveBufferToFile( const wxString filename, const wxString buffer, int eolType)
{
	wxTextFile file( filename );
	wxTextFileType tft = wxTextFileType_Dos;
	if ( file.Exists() ) {
		int ret = wxMessageBox( _( "File already exists!\n\n Overwrite?" ), _( "Generate class files" ), wxYES_NO | wxYES_DEFAULT | wxICON_EXCLAMATION );
		if ( ret == wxID_NO )
			return false;
	}
	switch ( m_curEol ) {
	case 0:
		tft = wxTextFileType_Dos;
		break;
	case 1:
		tft = wxTextFileType_Mac;
		break;
	case 2:
		tft = wxTextFileType_Unix;
		break;
	}
	file.Create();
	file.AddLine( buffer, tft );
	file.Write( tft );
	file.Close();
	return true;
}
