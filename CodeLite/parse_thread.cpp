//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// copyright            : (C) 2008 by Eran Ifrah
// file name            : parse_thread.cpp
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
#include "precompiled_header.h"
#include "tags_storage_sqlite3.h"
#include "pp_include.h"
#include "pptable.h"
#include "file_logger.h"
#include <wx/tokenzr.h>
#include "crawler_include.h"
#include "parse_thread.h"
#include "ctags_manager.h"
#include "istorage.h"
#include <wx/stopwatch.h>
#include <wx/xrc/xmlres.h>

#define DEBUG_MESSAGE(x) CL_DEBUG1(x.c_str())

#define TEST_DESTROY() {\
		if( TestDestroy() ) {\
			DEBUG_MESSAGE( wxString::Format(wxT("ParseThread::ProcessIncludes -> received 'TestDestroy()'") ) );\
			return;\
		}\
	}

const wxEventType wxEVT_PARSE_THREAD_UPDATED_FILE_SYMBOLS = XRCID("parse_thread_updated_symbols");
const wxEventType wxEVT_PARSE_THREAD_MESSAGE              = XRCID("parse_thread_update_status_bar");
const wxEventType wxEVT_PARSE_THREAD_SCAN_INCLUDES_DONE   = XRCID("parse_thread_scan_includes_done");
const wxEventType wxEVT_PARSE_THREAD_CLEAR_TAGS_CACHE     = XRCID("parse_thread_clear_tags_cache");
const wxEventType wxEVT_PARSE_THREAD_RETAGGING_PROGRESS   = XRCID("parse_thread_clear_retagging_progress");
const wxEventType wxEVT_PARSE_THREAD_RETAGGING_COMPLETED  = XRCID("parse_thread_clear_retagging_compelted");
const wxEventType wxEVT_PARSE_THREAD_INTERESTING_MACROS   = XRCID("parse_thread_interesting_macros_found");

ParseThread::ParseThread()
	: WorkerThread()
{
}

ParseThread::~ParseThread()
{
}

void ParseThread::ProcessRequest(ThreadRequest * request)
{
	// request is delete by the parent WorkerThread after this method is completed
	ParseRequest *req    = (ParseRequest*)request;

	switch (req->getType()) {
	case ParseRequest::PR_PARSEINCLUDES:
		ProcessIncludes( req );
		break;
	default:
	case ParseRequest::PR_FILESAVED:
		ProcessSimple( req );
		break;
	case ParseRequest::PR_PARSE_AND_STORE:
		ProcessParseAndStore( req );
		break;
	case ParseRequest::PR_DELETE_TAGS_OF_FILES:
		ProcessDeleteTagsOfFiles( req );
		break;
	case ParseRequest::PR_PARSE_FILE_NO_INCLUDES:
		ProcessSimpleNoIncludes( req );
		break;
	}
}

void ParseThread::ParseIncludeFiles(const wxString& filename, ITagsStoragePtr db)
{
	wxArrayString arrFiles;
	fcFileOpener::Instance()->ClearResults();
	GetFileListToParse(filename, arrFiles);
	int initalCount = arrFiles.GetCount();

	TEST_DESTROY();

	DEBUG_MESSAGE( wxString::Format(wxT("Files that need parse %u"), (unsigned int)arrFiles.GetCount()) ) ;
	TagsManagerST::Get()->FilterNonNeededFilesForRetaging(arrFiles, db);
	DEBUG_MESSAGE( wxString::Format(wxT("Actual files that need parse %u"), (unsigned int)arrFiles.GetCount()) );

	ParseAndStoreFiles(arrFiles, initalCount, db);
}

TagTreePtr ParseThread::DoTreeFromTags(const wxString& tags, int &count)
{
	return TagsManagerST::Get()->TreeFromTags(tags, count);
}

void ParseThread::DoStoreTags(const wxString& tags, const wxString &filename, int &count, ITagsStoragePtr db)
{
	TagTreePtr ttp = DoTreeFromTags(tags, count);
	db->Begin();
	db->DeleteByFileName( wxFileName(), filename, false);
	db->Store(ttp, wxFileName(), false);
	db->Commit();
}

void ParseThread::SendEvent(int evtType, const wxString &fileName, std::vector<std::pair<wxString, TagEntry> >  &items)
{
	SymbolTreeEvent event(items, evtType);
	event.SetFileName(fileName.c_str());
	wxPostEvent(m_notifiedWindow, event);
}

void ParseThread::SetCrawlerEnabeld(bool b)
{
	wxCriticalSectionLocker locker ( m_cs );
	m_crawlerEnabled = b;
}

void ParseThread::SetSearchPaths(const wxArrayString& paths, const wxArrayString &exlucdePaths)
{
	wxCriticalSectionLocker locker( m_cs );
	m_searchPaths.Clear();
	m_excludePaths.Clear();
	for (size_t i=0; i<paths.GetCount(); i++) {
		m_searchPaths.Add( paths.Item(i).c_str() );
	}

	for (size_t i=0; i<exlucdePaths.GetCount(); i++) {
		m_excludePaths.Add( exlucdePaths.Item(i).c_str() );
	}
}

bool ParseThread::IsCrawlerEnabled()
{
	wxCriticalSectionLocker locker( m_cs );
	return m_crawlerEnabled;
}

void ParseThread::GetSearchPaths(wxArrayString& paths, wxArrayString &excludePaths)
{
	wxCriticalSectionLocker locker( m_cs );
	for (size_t i=0; i<m_searchPaths.GetCount(); i++) {
		paths.Add( m_searchPaths.Item(i).c_str() );
	}

	for (size_t i=0; i<m_excludePaths.GetCount(); i++) {
		excludePaths.Add( m_excludePaths.Item(i).c_str() );
	}
}

void ParseThread::ProcessIncludes(ParseRequest* req)
{
	DEBUG_MESSAGE( wxString::Format(wxT("ProcessIncludes -> started")) ) ;

	FindIncludedFiles(req);
	std::set<std::string> *newSet = new std::set<std::string>(fcFileOpener::Instance()->GetResults());

#ifdef PARSE_THREAD_DBG
	std::set<std::string>::iterator iter = newSet->begin();
	for(; iter != newSet->end(); iter++) {
		wxString fileN((*iter).c_str(), wxConvUTF8);
		DEBUG_MESSAGE( wxString::Format(wxT("ParseThread::ProcessIncludes -> %s"), fileN.c_str() ) );
	}
#endif

	// collect the results
	wxCommandEvent event(wxEVT_PARSE_THREAD_SCAN_INCLUDES_DONE);
	event.SetClientData(newSet);
	event.SetInt((int)req->_quickRetag);
	wxPostEvent(req->_evtHandler, event);
}

void ParseThread::ProcessSimple(ParseRequest* req)
{
	wxString      dbfile = req->getDbfile();
	wxString      file   = req->getFile();

	// Skip binary file
	if(TagsManagerST::Get()->IsBinaryFile(file)) {
		DEBUG_MESSAGE( wxString::Format(wxT("Skipping binary file %s"), file.c_str()) );
		return;
	}

	// convert the file to tags
	TagsManager *tagmgr = TagsManagerST::Get();
	ITagsStoragePtr db(new TagsStorageSQLite());
	db->OpenDatabase( dbfile );

	//convert the file content into tags
	wxString tags;
	wxString file_name(req->getFile());
	tagmgr->SourceToTags(file_name, tags);

	int count;
	DoStoreTags(tags, file_name, count, db);

	db->Begin();
	///////////////////////////////////////////
	// update the file retag timestamp
	///////////////////////////////////////////
	db->InsertFileEntry(file, (int)time(NULL));

	////////////////////////////////////////////////
	// Parse and store the macros found in this file
	////////////////////////////////////////////////
	PPTable::Instance()->Clear();
	PPScan( file, true );
	db->StoreMacros( PPTable::Instance()->GetTable() );
	PPTable::Instance()->Clear();

	db->Commit();

	// Parse the saved file to get a list of files to include
	ParseIncludeFiles( file, db );

	// If there is no event handler set to handle this comaprison
	// results, then nothing more to be done
	if (m_notifiedWindow ) {
		// send "end" event
		wxCommandEvent e(wxEVT_PARSE_THREAD_UPDATED_FILE_SYMBOLS);
		wxPostEvent(m_notifiedWindow, e);

		wxCommandEvent clearCacheEvent(wxEVT_PARSE_THREAD_CLEAR_TAGS_CACHE);
		wxPostEvent(m_notifiedWindow, clearCacheEvent);
	}
}

void ParseThread::GetFileListToParse(const wxString& filename, wxArrayString& arrFiles)
{
	if ( !this->IsCrawlerEnabled() ) {
		return;
	}


	{
		wxCriticalSectionLocker locker( TagsManagerST::Get()->m_crawlerLocker );

		wxArrayString includePaths, excludePaths;
		GetSearchPaths( includePaths, excludePaths );

		fcFileOpener::Instance()->ClearSearchPath();
		for(size_t i=0; i<includePaths.GetCount(); i++) {
			fcFileOpener::Instance()->AddSearchPath( includePaths.Item(i).mb_str(wxConvUTF8).data() );
		}

		for(size_t i=0; i<excludePaths.GetCount(); i++) {
			fcFileOpener::Instance()->AddExcludePath(excludePaths.Item(i).mb_str(wxConvUTF8).data());
		}

		// Invoke the crawler
		const wxCharBuffer cfile = filename.mb_str(wxConvUTF8);

		// Skip binary files
		if(TagsManagerST::Get()->IsBinaryFile(filename)) {
			DEBUG_MESSAGE( wxString::Format(wxT("Skipping binary file %s"), filename.c_str()) );
			return;
		}

		// Before using the 'crawlerScan' we lock it, since it is not mt-safe
		crawlerScan( cfile.data() );

	}

	std::set<std::string> fileSet = fcFileOpener::Instance()->GetResults();
	std::set<std::string>::iterator iter = fileSet.begin();
	for (; iter != fileSet.end(); iter++ ) {
		wxFileName fn(wxString((*iter).c_str(), wxConvUTF8));
		fn.MakeAbsolute();
		if ( arrFiles.Index(fn.GetFullPath()) == wxNOT_FOUND ) {
			arrFiles.Add(fn.GetFullPath());
		}
	}
}

void ParseThread::ParseAndStoreFiles(const wxArrayString& arrFiles, int initalCount, ITagsStoragePtr db)
{
	// Loop over the files and parse them
	int totalSymbols (0);
	DEBUG_MESSAGE(wxString::Format(wxT("Parsing and saving files to database....")));
	for (size_t i=0; i<arrFiles.GetCount(); i++) {

		// give a shutdown request a chance
		TEST_DESTROY();

		wxString tags;  // output
		TagsManagerST::Get()->SourceToTags(arrFiles.Item(i), tags);

		if ( tags.IsEmpty() == false ) {
			DoStoreTags(tags, arrFiles.Item(i), totalSymbols, db);
		}
	}

	DEBUG_MESSAGE(wxString(wxT("Done")));

	// Update the retagging timestamp
	TagsManagerST::Get()->UpdateFilesRetagTimestamp(arrFiles, db);

	if ( m_notifiedWindow ) {
		wxCommandEvent e(wxEVT_PARSE_THREAD_MESSAGE);
		wxString message;
		if(initalCount != -1)
			message << wxT("INFO: Found ") << initalCount << wxT(" system include files. ");
		message << arrFiles.GetCount() << wxT(" needed to be parsed. Stored ") << totalSymbols << wxT(" new tags to the database");

		e.SetClientData(new wxString(message.c_str()));
		m_notifiedWindow->AddPendingEvent( e );

		// if we added new symbols to the database, send an even to the main thread
		// to clear the tags cache
		if(totalSymbols) {
			wxCommandEvent clearCacheEvent(wxEVT_PARSE_THREAD_CLEAR_TAGS_CACHE);
			m_notifiedWindow->AddPendingEvent(clearCacheEvent);
		}
	}
}

void ParseThread::ProcessDeleteTagsOfFiles(ParseRequest* req)
{
	DEBUG_MESSAGE(wxString(wxT("ParseThread::ProcessDeleteTagsOfFile")));
	if(req->_workspaceFiles.empty())
		return;

	wxString dbfile = req->getDbfile();
	ITagsStoragePtr db(new TagsStorageSQLite());

	db->OpenDatabase( dbfile );
	db->Begin();

	wxArrayString file_array;

	for (size_t i=0; i<req->_workspaceFiles.size(); i++) {
		wxString filename(req->_workspaceFiles.at(i).c_str(), wxConvUTF8);
		db->DeleteByFileName(wxFileName(),filename, false);
		file_array.Add(filename);
	}

	db->DeleteFromFiles(file_array);
	db->Commit();
	DEBUG_MESSAGE(wxString(wxT("ParseThread::ProcessDeleteTagsOfFile - completed")));
}

void ParseThread::ProcessParseAndStore(ParseRequest* req)
{
	wxString dbfile = req->getDbfile();

	// convert the file to tags
	double maxVal = (double)req->_workspaceFiles.size();
	if ( maxVal == 0.0 ) {
		return;
	}

	// we report every 10%
	double reportingPoint = maxVal / 100.0;
	reportingPoint = ceil( reportingPoint );
	if(reportingPoint == 0.0) {
		reportingPoint = 1.0;
	}

	ITagsStoragePtr db(new TagsStorageSQLite());
	db->OpenDatabase( dbfile );

	// We commit every 10 files
	db->Begin();
	int    precent               (0);
	int    lastPercentageReported(0);

	PPTable::Instance()->Clear();

	for (size_t i=0; i<maxVal; i++) {

		// give a shutdown request a chance
		if( TestDestroy() ) {
			// Do an ordered shutdown:
			// rollback any transaction
			// and close the database
			db->Rollback();
			return;
		}

		wxFileName curFile(wxString(req->_workspaceFiles.at(i).c_str(), wxConvUTF8));

		// Skip binary files
		if(TagsManagerST::Get()->IsBinaryFile(curFile.GetFullPath())) {
			DEBUG_MESSAGE( wxString::Format(wxT("Skipping binary file %s"), curFile.GetFullPath().c_str()) );
			continue;
		}

		// Send notification to the main window with our progress report
		precent = (int)((i / maxVal) * 100);

		if( m_notifiedWindow && lastPercentageReported !=  precent) {
			lastPercentageReported = precent;
			wxCommandEvent retaggingProgressEvent(wxEVT_PARSE_THREAD_RETAGGING_PROGRESS);
			retaggingProgressEvent.SetInt( (int)precent );
			m_notifiedWindow->AddPendingEvent(retaggingProgressEvent);

		} else if(lastPercentageReported !=  precent) {
			wxPrintf(wxT("parsing: %%%d completed\n"), precent);
		}

		TagTreePtr tree = TagsManagerST::Get()->ParseSourceFile(curFile);
		PPScan( curFile.GetFullPath(), false );

		db->Store(tree, wxFileName(), false);
		if(db->InsertFileEntry(curFile.GetFullPath(), (int)time(NULL)) == TagExist) {
			db->UpdateFileEntry(curFile.GetFullPath(), (int)time(NULL));
		}

		if ( i % 50 == 0 ) {
			// Commit what we got so far
			db->Commit();
			// Start a new transaction
			db->Begin();
		}
	}

	// Process the macros
	PPTable::Instance()->Squeeze();
	const std::map<wxString, PPToken>& table = PPTable::Instance()->GetTable();

	// Store the macros
	db->StoreMacros( table );

	// Commit whats left
	db->Commit();

	// Clear the results
	PPTable::Instance()->Clear();

	/// Send notification to the main window with our progress report
	if(m_notifiedWindow) {

		wxCommandEvent retaggingCompletedEvent(wxEVT_PARSE_THREAD_RETAGGING_COMPLETED);
		std::vector<std::string> *arrFiles = new std::vector<std::string>;
		*arrFiles = req->_workspaceFiles;
		retaggingCompletedEvent.SetClientData( arrFiles );
		m_notifiedWindow->AddPendingEvent(retaggingCompletedEvent);

	} else {
		wxPrintf(wxT("parsing: done\n"), precent);

	}
}

void ParseThread::FindIncludedFiles(ParseRequest *req)
{
	wxArrayString searchPaths, excludePaths, filteredFileList;
	GetSearchPaths( searchPaths, excludePaths );

	DEBUG_MESSAGE( wxString::Format(wxT("Initial workspace files count is %u"), (unsigned int)req->_workspaceFiles.size()) ) ;

	for(size_t i=0; i<req->_workspaceFiles.size(); i++) {
		wxString name(req->_workspaceFiles.at(i).c_str(), wxConvUTF8);
		wxFileName fn(name);
		fn.MakeAbsolute();

		if(TagsManagerST::Get()->IsBinaryFile(fn.GetFullPath()))
			continue;

		filteredFileList.Add( fn.GetFullPath() );
	}

	DEBUG_MESSAGE( wxString::Format(wxT("ParseThread::FindIncludedFiles -> Workspace files %u"), (unsigned int)filteredFileList.GetCount()) );

	wxArrayString arrFiles;

	// Clear the results once
	{
		// Before using the 'crawlerScan' we lock it, since it is not mt-safe
		wxCriticalSectionLocker locker( TagsManagerST::Get()->m_crawlerLocker );

		fcFileOpener::Instance()->ClearResults();
		fcFileOpener::Instance()->ClearSearchPath();

		for(size_t i=0; i<searchPaths.GetCount(); i++) {
			const wxCharBuffer path = _C(searchPaths.Item(i));
			DEBUG_MESSAGE( wxString::Format(wxT("ParseThread: Using Search Path: %s "), searchPaths.Item(i).c_str()) );
			fcFileOpener::Instance()->AddSearchPath(path.data());
		}

		for(size_t i=0; i<excludePaths.GetCount(); i++) {
			const wxCharBuffer path = _C(excludePaths.Item(i));
			DEBUG_MESSAGE( wxString::Format(wxT("ParseThread: Using Exclude Path: %s "), excludePaths.Item(i).c_str()) );
			fcFileOpener::Instance()->AddExcludePath(path.data());
		}

		for(size_t i=0; i<filteredFileList.GetCount(); i++) {
			const wxCharBuffer cfile = filteredFileList.Item(i).mb_str(wxConvUTF8);
			crawlerScan(cfile.data());
			if( TestDestroy() ) {
				DEBUG_MESSAGE( wxString::Format(wxT("ParseThread::FindIncludedFiles -> received 'TestDestroy()'") ) );
				return;
			}
		}
	}
}

//--------------------------------------------------------------------------------------
// Parse Request Class
//--------------------------------------------------------------------------------------
void ParseRequest::setDbFile(const wxString& dbfile)
{
	_dbfile = dbfile.c_str();
}

void ParseRequest::setTags(const wxString& tags)
{
	_tags = tags.c_str();
}

ParseRequest::ParseRequest(const ParseRequest& rhs)
{
	if (this == &rhs) {
		return;
	}
	*this = rhs;
}

ParseRequest &ParseRequest::operator =(const ParseRequest& rhs)
{
	setFile  (rhs._file.c_str()  );
	setDbFile(rhs._dbfile.c_str());
	setTags  (rhs._tags          );
	setType  (rhs._type          );
	return *this;
}

void ParseRequest::setFile(const wxString& file)
{
	_file = file.c_str();
}

ParseRequest::~ParseRequest()
{
}

// Adaptor to the parse thread
static ParseThread* gs_theParseThread = NULL;

void ParseThreadST::Free()
{
	if(gs_theParseThread) {
		delete gs_theParseThread;
	}
	gs_theParseThread = NULL;
}

ParseThread* ParseThreadST::Get()
{
	if(gs_theParseThread == NULL)
		gs_theParseThread = new ParseThread;
	return gs_theParseThread;
}

void ParseThread::ProcessSimpleNoIncludes(ParseRequest* req)
{
	std::vector<std::string> files  = req->_workspaceFiles;
	wxString                 dbfile = req->getDbfile();

	// Filter binary files
	std::vector<std::string> filteredFiles;
	wxArrayString filesArr;
	for(size_t i=0; i<files.size(); i++) {
		wxString filename = wxString(files.at(i).c_str(), wxConvUTF8);
		if(TagsManagerST::Get()->IsBinaryFile(filename))
			continue;
		filesArr.Add(filename);
	}
	
	// convert the file to tags
	ITagsStoragePtr db(new TagsStorageSQLite());
	db->OpenDatabase( dbfile );
	
	TagsManagerST::Get()->FilterNonNeededFilesForRetaging(filesArr, db);
	ParseAndStoreFiles(filesArr, -1, db);
	
	if(m_notifiedWindow) {
		wxCommandEvent e(wxEVT_PARSE_THREAD_RETAGGING_COMPLETED);
		e.SetClientData(NULL);
		wxPostEvent(m_notifiedWindow, e);
	}
}
