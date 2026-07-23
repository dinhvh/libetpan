/*
 * libEtPan! -- a mail stuff library
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailactivesync_codes.h"

#include <string.h>

static const struct mailactivesync_wbxml_token_info wbxml_tokens[] = {
  { MAILACTIVESYNC_CP_AIRSYNC, 0x05, "AirSync:Sync" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x06, "AirSync:Responses" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x07, "AirSync:Add" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x08, "AirSync:Change" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x09, "AirSync:Delete" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x0A, "AirSync:Fetch" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x0B, "AirSync:SyncKey" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x0C, "AirSync:ClientId" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x0D, "AirSync:ServerId" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x0E, "AirSync:Status" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x0F, "AirSync:Collection" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x10, "AirSync:Class" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x12, "AirSync:CollectionId" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x13, "AirSync:GetChanges" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x14, "AirSync:MoreAvailable" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x15, "AirSync:WindowSize" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x16, "AirSync:Commands" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x17, "AirSync:Options" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x18, "AirSync:FilterType" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x1B, "AirSync:Conflict" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x1C, "AirSync:Collections" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x1D, "AirSync:ApplicationData" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x1E, "AirSync:DeletesAsMoves" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x20, "AirSync:Supported" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x21, "AirSync:SoftDelete" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x22, "AirSync:MIMESupport" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x23, "AirSync:MIMETruncation" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x24, "AirSync:Wait" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x25, "AirSync:Limit" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x26, "AirSync:Partial" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x27, "AirSync:ConversationMode" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x28, "AirSync:MaxItems" },
  { MAILACTIVESYNC_CP_AIRSYNC, 0x29, "AirSync:HeartbeatInterval" },

  { MAILACTIVESYNC_CP_EMAIL, 0x0F, "Email:DateReceived" },
  { MAILACTIVESYNC_CP_EMAIL, 0x11, "Email:DisplayTo" },
  { MAILACTIVESYNC_CP_EMAIL, 0x12, "Email:Importance" },
  { MAILACTIVESYNC_CP_EMAIL, 0x13, "Email:MessageClass" },
  { MAILACTIVESYNC_CP_EMAIL, 0x14, "Email:Subject" },
  { MAILACTIVESYNC_CP_EMAIL, 0x15, "Email:Read" },
  { MAILACTIVESYNC_CP_EMAIL, 0x16, "Email:To" },
  { MAILACTIVESYNC_CP_EMAIL, 0x17, "Email:Cc" },
  { MAILACTIVESYNC_CP_EMAIL, 0x18, "Email:From" },
  { MAILACTIVESYNC_CP_EMAIL, 0x19, "Email:ReplyTo" },
  { MAILACTIVESYNC_CP_EMAIL, 0x1B, "Email:Flag" },

  { MAILACTIVESYNC_CP_MOVE, 0x05, "Move:Moves" },
  { MAILACTIVESYNC_CP_MOVE, 0x06, "Move:Move" },
  { MAILACTIVESYNC_CP_MOVE, 0x07, "Move:SrcMsgId" },
  { MAILACTIVESYNC_CP_MOVE, 0x08, "Move:SrcFldId" },
  { MAILACTIVESYNC_CP_MOVE, 0x09, "Move:DstFldId" },
  { MAILACTIVESYNC_CP_MOVE, 0x0A, "Move:Response" },
  { MAILACTIVESYNC_CP_MOVE, 0x0B, "Move:Status" },
  { MAILACTIVESYNC_CP_MOVE, 0x0C, "Move:DstMsgId" },

  { MAILACTIVESYNC_CP_GETITEMESTIMATE, 0x05, "GetItemEstimate:GetItemEstimate" },
  { MAILACTIVESYNC_CP_GETITEMESTIMATE, 0x06, "GetItemEstimate:Version" },
  { MAILACTIVESYNC_CP_GETITEMESTIMATE, 0x07, "GetItemEstimate:Collections" },
  { MAILACTIVESYNC_CP_GETITEMESTIMATE, 0x08, "GetItemEstimate:Collection" },
  { MAILACTIVESYNC_CP_GETITEMESTIMATE, 0x09, "GetItemEstimate:Class" },
  { MAILACTIVESYNC_CP_GETITEMESTIMATE, 0x0A, "GetItemEstimate:CollectionId" },
  { MAILACTIVESYNC_CP_GETITEMESTIMATE, 0x0B, "GetItemEstimate:DateTime" },
  { MAILACTIVESYNC_CP_GETITEMESTIMATE, 0x0C, "GetItemEstimate:Estimate" },
  { MAILACTIVESYNC_CP_GETITEMESTIMATE, 0x0D, "GetItemEstimate:Response" },
  { MAILACTIVESYNC_CP_GETITEMESTIMATE, 0x0E, "GetItemEstimate:Status" },

  { MAILACTIVESYNC_CP_FOLDERHIERARCHY, 0x07, "FolderHierarchy:DisplayName" },
  { MAILACTIVESYNC_CP_FOLDERHIERARCHY, 0x08, "FolderHierarchy:ServerId" },
  { MAILACTIVESYNC_CP_FOLDERHIERARCHY, 0x09, "FolderHierarchy:ParentId" },
  { MAILACTIVESYNC_CP_FOLDERHIERARCHY, 0x0A, "FolderHierarchy:Type" },
  { MAILACTIVESYNC_CP_FOLDERHIERARCHY, 0x0C, "FolderHierarchy:Status" },
  { MAILACTIVESYNC_CP_FOLDERHIERARCHY, 0x0E, "FolderHierarchy:Changes" },
  { MAILACTIVESYNC_CP_FOLDERHIERARCHY, 0x0F, "FolderHierarchy:Add" },
  { MAILACTIVESYNC_CP_FOLDERHIERARCHY, 0x10, "FolderHierarchy:Delete" },
  { MAILACTIVESYNC_CP_FOLDERHIERARCHY, 0x11, "FolderHierarchy:Update" },
  { MAILACTIVESYNC_CP_FOLDERHIERARCHY, 0x12, "FolderHierarchy:SyncKey" },
  { MAILACTIVESYNC_CP_FOLDERHIERARCHY, 0x13, "FolderHierarchy:FolderCreate" },
  { MAILACTIVESYNC_CP_FOLDERHIERARCHY, 0x14, "FolderHierarchy:FolderDelete" },
  { MAILACTIVESYNC_CP_FOLDERHIERARCHY, 0x15, "FolderHierarchy:FolderUpdate" },
  { MAILACTIVESYNC_CP_FOLDERHIERARCHY, 0x16, "FolderHierarchy:FolderSync" },
  { MAILACTIVESYNC_CP_FOLDERHIERARCHY, 0x17, "FolderHierarchy:Count" },

  { MAILACTIVESYNC_CP_PING, 0x05, "Ping:Ping" },
  { MAILACTIVESYNC_CP_PING, 0x07, "Ping:Status" },
  { MAILACTIVESYNC_CP_PING, 0x08, "Ping:HeartbeatInterval" },
  { MAILACTIVESYNC_CP_PING, 0x09, "Ping:Folders" },
  { MAILACTIVESYNC_CP_PING, 0x0A, "Ping:Folder" },
  { MAILACTIVESYNC_CP_PING, 0x0B, "Ping:Id" },
  { MAILACTIVESYNC_CP_PING, 0x0C, "Ping:Class" },
  { MAILACTIVESYNC_CP_PING, 0x0D, "Ping:MaxFolders" },

  { MAILACTIVESYNC_CP_PROVISION, 0x05, "Provision:Provision" },
  { MAILACTIVESYNC_CP_PROVISION, 0x06, "Provision:Policies" },
  { MAILACTIVESYNC_CP_PROVISION, 0x07, "Provision:Policy" },
  { MAILACTIVESYNC_CP_PROVISION, 0x08, "Provision:PolicyType" },
  { MAILACTIVESYNC_CP_PROVISION, 0x09, "Provision:PolicyKey" },
  { MAILACTIVESYNC_CP_PROVISION, 0x0A, "Provision:Data" },
  { MAILACTIVESYNC_CP_PROVISION, 0x0B, "Provision:Status" },
  { MAILACTIVESYNC_CP_PROVISION, 0x0C, "Provision:RemoteWipe" },
  { MAILACTIVESYNC_CP_PROVISION, 0x0D, "Provision:EASProvisionDoc" },

  { MAILACTIVESYNC_CP_AIRSYNCBASE, 0x05, "AirSyncBase:BodyPreference" },
  { MAILACTIVESYNC_CP_AIRSYNCBASE, 0x06, "AirSyncBase:Type" },
  { MAILACTIVESYNC_CP_AIRSYNCBASE, 0x07, "AirSyncBase:TruncationSize" },
  { MAILACTIVESYNC_CP_AIRSYNCBASE, 0x08, "AirSyncBase:AllOrNone" },
  { MAILACTIVESYNC_CP_AIRSYNCBASE, 0x0A, "AirSyncBase:Body" },
  { MAILACTIVESYNC_CP_AIRSYNCBASE, 0x0B, "AirSyncBase:Data" },
  { MAILACTIVESYNC_CP_AIRSYNCBASE, 0x0C, "AirSyncBase:EstimatedDataSize" },
  { MAILACTIVESYNC_CP_AIRSYNCBASE, 0x0D, "AirSyncBase:Truncated" },
  { MAILACTIVESYNC_CP_AIRSYNCBASE, 0x0E, "AirSyncBase:Attachments" },
  { MAILACTIVESYNC_CP_AIRSYNCBASE, 0x0F, "AirSyncBase:Attachment" },
  { MAILACTIVESYNC_CP_AIRSYNCBASE, 0x10, "AirSyncBase:DisplayName" },
  { MAILACTIVESYNC_CP_AIRSYNCBASE, 0x11, "AirSyncBase:FileReference" },
  { MAILACTIVESYNC_CP_AIRSYNCBASE, 0x12, "AirSyncBase:Method" },
  { MAILACTIVESYNC_CP_AIRSYNCBASE, 0x13, "AirSyncBase:ContentId" },
  { MAILACTIVESYNC_CP_AIRSYNCBASE, 0x14, "AirSyncBase:ContentLocation" },
  { MAILACTIVESYNC_CP_AIRSYNCBASE, 0x15, "AirSyncBase:IsInline" },
  { MAILACTIVESYNC_CP_AIRSYNCBASE, 0x16, "AirSyncBase:NativeBodyType" },
  { MAILACTIVESYNC_CP_AIRSYNCBASE, 0x17, "AirSyncBase:ContentType" },
  { MAILACTIVESYNC_CP_AIRSYNCBASE, 0x18, "AirSyncBase:Preview" },

  { MAILACTIVESYNC_CP_SETTINGS, 0x05, "Settings:Settings" },
  { MAILACTIVESYNC_CP_SETTINGS, 0x06, "Settings:Status" },
  { MAILACTIVESYNC_CP_SETTINGS, 0x07, "Settings:Get" },
  { MAILACTIVESYNC_CP_SETTINGS, 0x08, "Settings:Set" },
  { MAILACTIVESYNC_CP_SETTINGS, 0x16, "Settings:DeviceInformation" },
  { MAILACTIVESYNC_CP_SETTINGS, 0x17, "Settings:Model" },
  { MAILACTIVESYNC_CP_SETTINGS, 0x18, "Settings:IMEI" },
  { MAILACTIVESYNC_CP_SETTINGS, 0x19, "Settings:FriendlyName" },
  { MAILACTIVESYNC_CP_SETTINGS, 0x1A, "Settings:OS" },
  { MAILACTIVESYNC_CP_SETTINGS, 0x1B, "Settings:OSLanguage" },
  { MAILACTIVESYNC_CP_SETTINGS, 0x1C, "Settings:PhoneNumber" },
  { MAILACTIVESYNC_CP_SETTINGS, 0x20, "Settings:UserAgent" },
  { MAILACTIVESYNC_CP_SETTINGS, 0x22, "Settings:MobileOperator" },

  { MAILACTIVESYNC_CP_ITEMOPERATIONS, 0x05, "ItemOperations:ItemOperations" },
  { MAILACTIVESYNC_CP_ITEMOPERATIONS, 0x06, "ItemOperations:Fetch" },
  { MAILACTIVESYNC_CP_ITEMOPERATIONS, 0x07, "ItemOperations:Store" },
  { MAILACTIVESYNC_CP_ITEMOPERATIONS, 0x08, "ItemOperations:Options" },
  { MAILACTIVESYNC_CP_ITEMOPERATIONS, 0x09, "ItemOperations:Range" },
  { MAILACTIVESYNC_CP_ITEMOPERATIONS, 0x0A, "ItemOperations:Total" },
  { MAILACTIVESYNC_CP_ITEMOPERATIONS, 0x0B, "ItemOperations:Properties" },
  { MAILACTIVESYNC_CP_ITEMOPERATIONS, 0x0C, "ItemOperations:Data" },
  { MAILACTIVESYNC_CP_ITEMOPERATIONS, 0x0D, "ItemOperations:Status" },
  { MAILACTIVESYNC_CP_ITEMOPERATIONS, 0x0E, "ItemOperations:Response" },
  { MAILACTIVESYNC_CP_ITEMOPERATIONS, 0x0F, "ItemOperations:Version" },
  { MAILACTIVESYNC_CP_ITEMOPERATIONS, 0x10, "ItemOperations:Schema" },
  { MAILACTIVESYNC_CP_ITEMOPERATIONS, 0x11, "ItemOperations:Part" },
  { MAILACTIVESYNC_CP_ITEMOPERATIONS, 0x12, "ItemOperations:EmptyFolderContents" },
  { MAILACTIVESYNC_CP_ITEMOPERATIONS, 0x13, "ItemOperations:DeleteSubFolders" },
  { MAILACTIVESYNC_CP_ITEMOPERATIONS, 0x16, "ItemOperations:Move" },
  { MAILACTIVESYNC_CP_ITEMOPERATIONS, 0x17, "ItemOperations:DstFldId" },

  { MAILACTIVESYNC_CP_COMPOSEMAIL, 0x05, "ComposeMail:SendMail" },
  { MAILACTIVESYNC_CP_COMPOSEMAIL, 0x06, "ComposeMail:SmartForward" },
  { MAILACTIVESYNC_CP_COMPOSEMAIL, 0x07, "ComposeMail:SmartReply" },
  { MAILACTIVESYNC_CP_COMPOSEMAIL, 0x08, "ComposeMail:SaveInSentItems" },
  { MAILACTIVESYNC_CP_COMPOSEMAIL, 0x09, "ComposeMail:ReplaceMime" },
  { MAILACTIVESYNC_CP_COMPOSEMAIL, 0x0B, "ComposeMail:Source" },
  { MAILACTIVESYNC_CP_COMPOSEMAIL, 0x0C, "ComposeMail:FolderId" },
  { MAILACTIVESYNC_CP_COMPOSEMAIL, 0x0D, "ComposeMail:ItemId" },
  { MAILACTIVESYNC_CP_COMPOSEMAIL, 0x0E, "ComposeMail:LongId" },
  { MAILACTIVESYNC_CP_COMPOSEMAIL, 0x0F, "ComposeMail:InstanceId" },
  { MAILACTIVESYNC_CP_COMPOSEMAIL, 0x10, "ComposeMail:Mime" },
  { MAILACTIVESYNC_CP_COMPOSEMAIL, 0x11, "ComposeMail:ClientId" },
  { MAILACTIVESYNC_CP_COMPOSEMAIL, 0x12, "ComposeMail:Status" },
  { MAILACTIVESYNC_CP_COMPOSEMAIL, 0x13, "ComposeMail:AccountId" }
};

const char * mailactivesync_wbxml_tag_name(uint8_t code_page,
    uint8_t token)
{
  unsigned int i;

  for (i = 0; i < sizeof(wbxml_tokens) / sizeof(wbxml_tokens[0]); i ++) {
    if ((wbxml_tokens[i].code_page == code_page) &&
        (wbxml_tokens[i].token == token))
      return wbxml_tokens[i].name;
  }

  return NULL;
}

int mailactivesync_wbxml_tag_lookup(const char * name,
    uint8_t * code_page,
    uint8_t * token)
{
  unsigned int i;

  if ((name == NULL) || (code_page == NULL) || (token == NULL))
    return 0;

  for (i = 0; i < sizeof(wbxml_tokens) / sizeof(wbxml_tokens[0]); i ++) {
    if (strcmp(wbxml_tokens[i].name, name) == 0) {
      * code_page = wbxml_tokens[i].code_page;
      * token = wbxml_tokens[i].token;
      return 1;
    }
  }

  return 0;
}
