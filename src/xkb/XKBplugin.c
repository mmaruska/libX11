
/* The Xlib part of setting plugins */

#include <stdio.h>
#define NEED_REPLIES
#define NEED_EVENTS
#include "Xlibint.h"
#include <X11/extensions/XKBproto.h>
#include "XKBlibint.h"



#if (MMC_PIPELINE || 1)

Bool
XkbSetPlugin(Display *dpy, unsigned int deviceSpec, const char* plugin_name,
	     const char* around, Bool before) /* If name is nul, remove the AROUND! */
{
   register xkbSetPluginReq *req;
   int name_len = strlen(plugin_name);
   int string_size = name_len + strlen(around) + 2; /* divisible by 4 ?? */

   /* a multiple of 4.  reminder  */
/* todo:
   Use this macro:
   #define	XkbPaddedSize(n)	((((unsigned int)(n)+3) >> 2) << 2)
*/
   int remainder = (string_size + SIZEOF(xkbSetPluginReq)) % 4;
   int string_size_aligned = string_size + (remainder?(4 - remainder):0);

   if ((dpy->flags & XlibDisplayNoXkb) ||
       (!dpy->xkb_info && !XkbUseExtension(dpy,NULL,NULL)))
      return False;

   /* mmc: with the lock do as little as possible! */
   LockDisplay(dpy);

   GetReqExtra(kbSetPlugin, string_size_aligned,req);       /* this makes wrong reqType */
   bzero(req,SIZEOF(xkbSetPluginReq) + string_size_aligned ); /* this seems stupid. !!! */

   req->reqType = dpy->xkb_info->codes->major_opcode;
   req->length = ((SIZEOF(xkbSetPluginReq) + string_size_aligned)>>2);
   req->xkbReqType = X_kbSetPlugin;
   req->deviceSpec = deviceSpec;
   req->before = before;


   memcpy(&(req->names), plugin_name, name_len + 1); /* event the 0 is copied  */

   strcpy(  ((char *) &(req->names)) + name_len + 1, around); /* including! */

#ifdef DEBUG
   printf ("%s: req length: %d, string_size: %d, string_size_aligned %d %s\n", __FUNCTION__,
	   req->length, string_size, string_size_aligned,
           &(req->names));
#endif
   UnlockDisplay(dpy);

   SyncHandle();
   return True;
}

/* This accepts a Xreply.
   It either extracts the 3 data slots into return_numbers,
   or gets the extended XReply, which is returned in a freshly allocated mem. */
static
int
XkbPluginAcceptReply(Display *dpy, int plugin, int *length, char** data, int *return_numbers)
{
   /* DPY IS LOCKED! it will be unlocked by the caller! */
   XKBpluginReply  rep;
   XkbReadBufferRec buf;

#if 0
   /* todo: XOR! */
   assert(!return_numbers || !data);
   assert(return_numbers || data);
#endif

   /* In the current version, `xGenericReply' has a lot of space to transmit 4 bytes. I use it!  */


   if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) { /* fixme:  (SIZEOF(XKBpluginReply) - SIZEOF(xGenericReply) >>2)  */
#if 0
      printf("%s bad implementation: the plugin didn't return an XReply\n", __FUNCTION__);
#endif
      return BadImplementation;
   }

   if (rep.length == 0)
      {
         if(return_numbers)
            {
               memcpy(return_numbers, &rep.data00, 3*sizeof(CARD32));
#if 0
               printf("%s: %d %d %d\n", __FUNCTION__,return_numbers[0],return_numbers[1],return_numbers[2]);
#endif
               return Success;
            }
         else
            {
               *length = 0;
               *data = NULL;
            }
         return Success;
      }

   /* Extended: */
   if (return_numbers) {
      _XEatData(dpy, rep.length << 2);
      return BadImplementation; /* a different kind of error! */
   }

#if 0
   printf("%s _XkbInitReadBuffer for %d bytes, real len = %d\n", __FUNCTION__, rep.length << 2,  *length);
#endif
   /* again, i abuse rep.data00: i need 2 bytes, for the real length! */
   if (_XkbInitReadBuffer(dpy,&buf, rep.length << 2)) /* !! */
      {
         char* s;
         *length = (int) rep.real_len;  /* fixme! */
         s = _XkbAlloc(*length);
         /* if (!s) out_of_memory! */
         *data = s;
         memcpy(s, buf.start, *length);

         /* *events = (archived_event *) buf.start;*/
         _XkbFreeReadBuffer(&buf);
         return Success;
      }
   else
      {
         return BadImplementation;
      }
}


/* get_reply ->  */
/* old: const char *plugin_name  */
static int
XkbPluginConfigure_start(Display *dpy, unsigned int deviceSpec, unsigned int plugin,
			 int data[5], int get)
{
   register xkbPluginConfigReq *req;


   if ((dpy->flags & XlibDisplayNoXkb) ||
       (!dpy->xkb_info && !XkbUseExtension(dpy,NULL,NULL)))
      return False;


   LockDisplay(dpy);

   /* what is this ? */
   GetReq(kbPluginSetConfig,req);       /* this makes wrong reqType */

   bzero(req,SIZEOF(xkbPluginSetConfigReq)); /* this seems stupid. !!! */

   req->reqType = dpy->xkb_info->codes->major_opcode;
   req->deviceSpec = deviceSpec;     /* deviceSpec */
   req->xkbReqType = get? X_kbPluginGetConfig: X_kbPluginSetConfig;

   req->length = SIZEOF(xkbPluginGetConfigReq) >> 2; /* same for Set! */
   req->plugin_id = plugin;

   /* fixme: 64bit & endianess! */
   memcpy(&(req->data0), data, 5 * 4); /* 12 bytes    sizeof(data)*/
#if 0
   printf("%s: %d %d %d %d\n", __FUNCTION__, req->data0,req->data1,req->data2,req->data3);
#endif

   if (!get)
      {
         UnlockDisplay(dpy);
      }
   /* SyncHandle(); */
   return Success;
}


int
XkbPluginConfigure(Display *dpy, unsigned int deviceSpec, unsigned int plugin, int data[5])
{
   return XkbPluginConfigure_start(dpy, deviceSpec, plugin, data, 0);
}

int
XkbPluginGetConfigure(Display *dpy, unsigned int deviceSpec, unsigned int plugin, int data[5],
		      int ret[3])
{
   int status = XkbPluginConfigure_start(dpy, deviceSpec, plugin, data, 1);
   if (status != Success)
      {
         UnlockDisplay(dpy);
         return status;
      }

   /* fixme: Check for errors: */
   XkbPluginAcceptReply(dpy, plugin, 0, NULL, ret);
   UnlockDisplay(dpy);
   return Success;
}







/* get_reply ->  */
/* old: const char *plugin_name  */
int
XkbPluginCommand(Display *dpy, unsigned int deviceSpec, int plugin, int len, char *data,
                 /* LEN is the lenght of DATA */
                 /* ANSWER is xmalloc-ed ! and ANSWER_LEN is filled w/ the lenght of that memory.  */
                 int *answer_len, char** answer)
{
   register xkbPluginCommandReq *req;
   int status;

   if ((dpy->flags & XlibDisplayNoXkb) ||
       (!dpy->xkb_info && !XkbUseExtension(dpy,NULL,NULL)))
      return False;

   LockDisplay(dpy);

   /* what is this ? */
   GetReqExtra(kbPluginCommand, len, req);
   bzero(req,SIZEOF(xkbPluginGetConfigReq)); /* fixme! */

   /* int len_aligned = ((len +3)>>2);*/


   req->reqType = dpy->xkb_info->codes->major_opcode;
   req->length = (len + 3 + SIZEOF(xkbPluginCommandReq))  >>2;
   /* fixme! (SIZEOF(xkbPluginGetConfigReq)>>2)*/

   req->xkbReqType = X_kbPluginCommand;
   req->deviceSpec = deviceSpec;     /* deviceSpec */

   req->plugin_id = plugin;
   memcpy(req + SIZEOF(xkbPluginCommandReq), data, len);
   /* pity that we cannot send off the data! */

   status = XkbPluginAcceptReply(dpy, plugin, answer_len, answer, NULL);

   UnlockDisplay(dpy);
   /* SyncHandle();*/
   return status;
}


#endif /* MMC_PIPELINE */


/* fixme!  */
#if MMC_PIPELINE || 1
/* Code copied & modified from XKBlib.h */

static void
_FreePipelineInfo(int num, XkbPluginInfo* names)
{
int		i;
XkbPluginInfo*	tmp;

    if ((num<1)||(names==NULL))
	return;

    for (i=0,tmp=names;i<num;i++,tmp++) {
	if (tmp->name) {
	    _XkbFree(tmp->name);
	    tmp->name= NULL;
	}
    }
    _XkbFree(names);
    return;
}


void
XkbFreePipelineList(XkbPipelineListPtr list)
{
    if (list) {
	if (list->plugins)
	    _FreePipelineInfo(list->num_plugins,list->plugins);

	/* bzero((char *)list,sizeof(XkbPipelineListRec)); */
	_XkbFree(list);
    }
    return;
}

#define DEBUG 0                 /* fixme! */
static XkbPluginInfo* //XkbComponentNamePtr
/* hacky :( */
_ReadNumberedListing(XkbReadBufferPtr buf,int count,Status *status_rtrn)
{
    /* [len] [string .....] [len] [string.....] ....... */
   /* XkbComponentNamePtr XkbComponentNameRec XkbComponentNamePtr*/
    XkbPluginInfo* this;
    XkbPluginInfo* first= _XkbTypedCalloc(count,XkbPluginInfo);

    int i;
    char* str;
    CARD8* flags;  /* 16 */

    if (!first)
        return NULL;

#if DEBUG
    printf("%s: will read %d names\n", __FUNCTION__, count);
#endif

    for (this=first,i=0;i<count;i++,this++)
    {
	int slen;
	/* ~/xfree/xc/lib/X11/XKBRdBuf.c */
	_XkbCopyFromReadBuffer(buf, (char*) &this->id, 4); /* endianess! */
	/* this->id= _XkbGetReadBufferPtr(buf,sizeof(CARD8)); */


	flags = (CARD8 *)_XkbGetReadBufferPtr(buf,sizeof(CARD8)); /* fixme! */
	if (flags == 0) {
#if DEBUG
	    printf("%s: received 0! aborting\n", __FUNCTION__);
#endif
	    goto BAILOUT;
	}
	slen = *flags;
	this->name =	_XkbTypedCalloc(slen+1,char);
	if (!this->name)
	    goto BAILOUT;


	str = (char *)_XkbGetReadBufferPtr(buf,slen); /* ending \0 */
	memcpy(this->name,str,slen);
	this->name[slen] = 0;
#if DEBUG
	printf("%s: %d = %s\n", __FUNCTION__, this->id, this->name);
#endif
    }
    return first;

  BAILOUT:
    *status_rtrn= BadAlloc;
    /* fixme! */
    _FreePipelineInfo(i,first);
    return NULL;
}


XkbPipelineListPtr
XkbListPipeline(	Display *		dpy,
			unsigned		deviceSpec
			)
{
register xkbListPipelineReq*	req;
xkbListPipelineReply 		rep;
XkbInfoPtr 			xkbi;
XkbPipelineListPtr		list;
XkbReadBufferRec		buf;
int extraLen;

    if ( (dpy==NULL) || (dpy->flags & XlibDisplayNoXkb) ||
         (!dpy->xkb_info && !XkbUseExtension(dpy,NULL,NULL)))
         return NULL;


    xkbi= dpy->xkb_info;
    LockDisplay(dpy);
    GetReq(kbListPipeline, req);
    req->reqType = 	xkbi->codes->major_opcode;
    req->xkbReqType = 	X_kbListPipeline;
    req->deviceSpec = 	deviceSpec;


    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse))
	goto BAILOUT;
    extraLen= (int)rep.length*4;

    if (extraLen==0) { /* no matches, but we don't want to report a failure */
	list= _XkbTypedCalloc(1,XkbPipelineListRec); /* full of zeros! */
	UnlockDisplay(dpy);
	SyncHandle();
	return list;
    }
    if (_XkbInitReadBuffer(dpy,&buf,extraLen)) {
	Status status;
	status= Success;
	list= _XkbTypedCalloc(1,XkbPipelineListRec);
	if (!list) {
	    _XkbFreeReadBuffer(&buf);
	    goto BAILOUT;
	}
        /* fixme: where is the Rep  */
        list->num_plugins = rep.nPlugins;
	if ((status==Success)&&(list->num_plugins>0)) {
            list->plugins = _ReadNumberedListing(&buf, list->num_plugins,&status);
        }

	/* left= _XkbFreeReadBuffer(&buf);*/
	if ((status!=Success)||(buf.error)) { /* ||(left>2) */
	    XkbFreePipelineList(list);
	    goto BAILOUT;
	}
	UnlockDisplay(dpy);
	SyncHandle();
	return list;
    }
BAILOUT:
    UnlockDisplay(dpy);
    SyncHandle();
    return NULL;
}

#endif /* MMC_PIPELINE */
