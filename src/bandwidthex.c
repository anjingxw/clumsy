// bandwidth cap module
#include <stdlib.h>
#include <Windows.h>
#include <stdint.h>

#include "iup.h"
#include "common.h"

#define NAME "bandwidthex"
#define BANDWIDTHEX_MIN  "0"
#define BANDWIDTHEX_MAX  "99999"
#define BANDWIDTHEX_DEFAULT 10


//---------------------------------------------------------------------
// rate stats
//---------------------------------------------------------------------
static uint32_t timing_last_ts = 0;
static float timing_left_size = 0.0;
static PacketNode  bandwidth_head_node = {0}, bandwidth_tail_node = {0};
static PacketNode *buf_head_ = &bandwidth_head_node, *buf_tail_ = &bandwidth_tail_node;
static int buf_size_ = 0;

//---------------------------------------------------------------------
// configuration
//---------------------------------------------------------------------
static Ihandle *inboundCheckbox, *outboundCheckbox, *bandwidthInput;

static volatile short bandwidthExEnabled = 0,
    bandwidthExInbound = 1, bandwidthExOutbound = 1;

static volatile LONG bandwidthExLimit = BANDWIDTHEX_DEFAULT; 


static Ihandle* bandwidthEXSetupUI() {
    Ihandle *bandwidthControlsBox = IupHbox(
        inboundCheckbox = IupToggle("Inbound", NULL),
        outboundCheckbox = IupToggle("Outbound", NULL),
        IupLabel("Limit(KB/s):"),
        bandwidthInput = IupText(NULL),
        NULL
    );

    IupSetAttribute(bandwidthInput, "VISIBLECOLUMNS", "4");
    IupSetAttribute(bandwidthInput, "VALUE", STR(BANDWIDTHEX_DEFAULT));
    IupSetCallback(bandwidthInput, "VALUECHANGED_CB", uiSyncInt32);
    IupSetAttribute(bandwidthInput, SYNCED_VALUE, (char*)&bandwidthExLimit);
    IupSetAttribute(bandwidthInput, INTEGER_MAX, BANDWIDTHEX_MAX);
    IupSetAttribute(bandwidthInput, INTEGER_MIN, BANDWIDTHEX_MIN);
    IupSetCallback(inboundCheckbox, "ACTION", (Icallback)uiSyncToggle);
    IupSetAttribute(inboundCheckbox, SYNCED_VALUE, (char*)&bandwidthExInbound);
    IupSetCallback(outboundCheckbox, "ACTION", (Icallback)uiSyncToggle);
    IupSetAttribute(outboundCheckbox, SYNCED_VALUE, (char*)&bandwidthExOutbound);

    // enable by default to avoid confusing
    IupSetAttribute(inboundCheckbox, "VALUE", "ON");
    IupSetAttribute(outboundCheckbox, "VALUE", "ON");

    if (parameterized) {
        setFromParameter(inboundCheckbox, "VALUE", NAME"-inbound");
        setFromParameter(outboundCheckbox, "VALUE", NAME"-outbound");
        setFromParameter(bandwidthInput, "VALUE", NAME"-bandwidth");
    }

    return bandwidthControlsBox;
}

static void bandwidthEXStartUp() {
	if (buf_head_->next == NULL && buf_tail_->next == NULL) {
        buf_head_->next = buf_tail_;
        buf_tail_->prev = buf_head_;
        buf_size_ = 0;
    }
    LOG("bandwidthEx enabled");
}

static void bandwidthExCloseDown(PacketNode *head, PacketNode *tail) {
    UNREFERENCED_PARAMETER(head);
    UNREFERENCED_PARAMETER(tail);
    LOG("bandwidthEx disabled");
}


//---------------------------------------------------------------------
// process
//---------------------------------------------------------------------
static short bandwidthExProcess(PacketNode *head, PacketNode* tail) {
	DWORD now_ts = timeGetTime();
	//限制的字节数
	int limit = bandwidthExLimit * 1024;

	int dropped = 0;
    while (head->next != tail) {
        PacketNode *pac = head->next;
        // chance in range of [0, 10000]
        if (checkDirection(pac->addr.Outbound, bandwidthExInbound, bandwidthExOutbound)) {
			if(buf_size_ > 2000){
				dropped++;
				freeNode(popNode(pac));
				LOG("drop buf_size_ = %d", buf_size_);
			}
			else{
				insertAfter(popNode(pac), buf_head_);
				++buf_size_;
				LOG("send buf_size_ = %d", buf_size_);
			}
    	} else {
            head = head->next;
    	}
    }

	if(timing_last_ts == 0){
		timing_last_ts = now_ts;
		timing_left_size = 0.0;
	}

	int diff  = now_ts - timing_last_ts;
	timing_last_ts = now_ts;
	timing_left_size += diff *limit/1000.0f;
	if(timing_left_size > limit){
		timing_left_size = limit;
	}
	//LOG("%d, %f", diff, timing_left_size);

	PacketNode * buf_head = buf_head_;
  	while (buf_head->next != buf_tail_) {
        PacketNode *pac = buf_head->next;
		if(pac->packetLen <= timing_left_size){
			timing_left_size -= pac->packetLen;
			insertAfter(popNode(pac), head)->timestamp = now_ts;
			--buf_size_;
		}
		else{
			break;
		}
    }	
	return dropped > 0 || buf_size_ > 0;
}


//---------------------------------------------------------------------
// module
//---------------------------------------------------------------------
Module bandwidthExModule = {
    "BandwidthQueue",
    NAME,
    (short*)&bandwidthExEnabled,
    bandwidthEXSetupUI,
    bandwidthEXStartUp,
    bandwidthExCloseDown,
    bandwidthExProcess,
    // runtime fields
    0, 0, NULL
};

