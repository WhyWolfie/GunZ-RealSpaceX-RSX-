#include "stdafx.h"
#include "MMatchNotify.h"
#include "MXml.h"
#include <crtdbg.h>
#include <map>

#include "../FileSystem/FileSystem.h"

#define MESSAGES_FILE_NAME	"system/notify.xml"

#define ZTOK_MESSAGE		"NOTIFY"
#define ZTOK_ID				"id"

typedef map<int, string>	MNotifyMap;

MNotifyMap g_NotifyMap;

bool InitializeNotify(MZFileSystem *pfs)
{
	std::unique_ptr<IFile> pFile = std::unique_ptr<IFile>(getFileManager()->open(MESSAGES_FILE_NAME));
    if (!pFile.get())
    {
        return false;
    }

    std::unique_ptr<char> buffer = std::unique_ptr<char>(new char[pFile->getSize()+1]);
    pFile->read(pFile->getSize(), buffer.get());
	buffer.get()[pFile->getSize()] = 0;

	MXmlDocument aXml;
	aXml.Create();
	if(!aXml.LoadFromMemory(buffer.get()))
	{
		return false;
	}
	buffer.reset();

	int iCount, i;
	MXmlElement		aParent, aChild;
	aParent = aXml.GetDocumentElement();
	iCount = aParent.GetChildNodeCount();

	char szTagName[256];
	for (i = 0; i < iCount; i++)
	{
		aChild = aParent.GetChildNode(i);
		aChild.GetTagName(szTagName);
		if(stricmp(szTagName,ZTOK_MESSAGE)==0)
		{
			int nID=0;
			if(aChild.GetAttribute(&nID,ZTOK_ID))
			{
				// 이미 등록되어있는게 없어야 한다. ( 메시지 중복 )
				//_ASSERT(g_NotifyMap.find(nID)==g_NotifyMap.end());		
				
				char szContents[256];
				aChild.GetContents(szContents);

				g_NotifyMap.insert(MNotifyMap::value_type(nID,string(MGetStringResManager()->GetStringFromXml(szContents))));
			}
		}
	}
	return true;
}

bool NotifyMessage(int nMsgID, string *out)
{
	MNotifyMap::iterator i=g_NotifyMap.find(nMsgID);
	if(i==g_NotifyMap.end())
		return false;

	*out = i->second;

	return true;
}