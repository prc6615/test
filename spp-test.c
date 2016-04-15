/*******************************************************************************
* spp-client.c
*  SPP client
* 
* Compile: sudo gcc spp-client.c -lbluetooth -o spp-client
* 
* Description:
*   手機作為藍牙 SPP Server，讓 藍牙 SPP Client 可以連接並互傳訊息。
* 
* How to test: 
*   Raspberry Pi + USB Bluetooth Dongle <--> Android phone
*  1.) 開啟手機 BTSCmode，並開啟藍牙
*  2.) 在樹莓派命令提示視窗下輸入 sudo ./spp-client
*   成功連線上了之後，會由樹莓派送出 hello! (10秒內) 的字串到手機上面去，這就表示
*   可以開始從手機傳送文字訊息了。每當手機傳送訊息出去讓樹莓派收到後，就會將收到的
*   字串顯示在螢幕上，並且回傳 ATOK 到手機螢幕上。 
* 
* Note: BTSCmode 藍牙若是沒有一開始就開啟而是使用 Open Bluetooth 打開，當執行 
*   sudo ./spp-client 會看不到任何輸出就結束。只要在藍牙重新開啟時等待一段時間，
*   之後的操作就會很順利，不需要再做等待，只要重複上面兩個步驟就可以了。
* 
* Result: OK !!!
*  連接上 BTSCmode SPP Server 後，會先輸出所連結的 RFCOMM channel number，並且在
*   10 秒鐘之內會在手機上顯示 hello! 之後就可以開始傳送訊息到樹莓派，並且每次手機
*   傳送字串之後，也會收到 ATOK 的樹莓派回應字串。
*  若再一次輸入 sudo ./spp-client，手機視窗上並不會繼續輸出任何字串；要返回
*  前一個畫面再次按下 AS Server 按鈕，重新輸入 sudo ./spp-client 再連接。
* 
* ****************************************************************************/
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <sys/socket.h>
#include <bluetooth/rfcomm.h>

//-- 搜尋遠端 SPP Server 所使用的 RFCOMM Port Number
// 回傳 RFCOMM Port Number
uint8_t get_rfcomm_port_number( const char bta[] )
{
    int status;
    bdaddr_t target;
    uuid_t svc_uuid;
    sdp_list_t *response_list, *search_list, *attrid_list;
    sdp_session_t *session = 0;
    uint32_t range = 0x0000ffff;
    uint8_t port = 0;

    //printf("g1\n");
    str2ba( bta, &target );

    //printf("g2\n");
    // connect to the SDP server running on the remote machine
    session = sdp_connect( BDADDR_ANY, &target, 0 );

    //printf("g3\n");
    sdp_uuid16_create( &svc_uuid, RFCOMM_UUID );
    search_list = sdp_list_append( 0, &svc_uuid );
    attrid_list = sdp_list_append( 0, &range );

    //printf("g4\n");
    // get a list of service records that have UUID 0xabcd
    response_list = NULL;
    status = sdp_service_search_attr_req( session, search_list,
            SDP_ATTR_REQ_RANGE, attrid_list, &response_list);

    //printf("g5\n");
    if( status == 0 ) {
        sdp_list_t *proto_list = NULL;
        sdp_list_t *r = response_list;
        
        // go through each of the service records
        for (; r; r = r->next ) {
            sdp_record_t *rec = (sdp_record_t*) r->data;
            
            // get a list of the protocol sequences
            if( sdp_get_access_protos( rec, &proto_list ) == 0 ) {

                // get the RFCOMM port number
                port = sdp_get_proto_port( proto_list, RFCOMM_UUID );

                sdp_list_free( proto_list, 0 );
            }
            sdp_record_free( rec );
        }
    }
    sdp_list_free( response_list, 0 );
    sdp_list_free( search_list, 0 );
    sdp_list_free( attrid_list, 0 );
    sdp_close( session );

    //printf("g6\n");
    if( port != 0 ) {
        printf("found service running on RFCOMM port %d\n", port);
    }
    
    return port;
}

int main(int argc, char **argv)
{
    struct sockaddr_rc addr = { 0 };
    int status, len, rfcommsock;
    char rfcommbuffer[255];
    char dest[18] = "34:81:F4:11:0B:16"; // 藍牙 SPP Server 的位址
    int i=0;    

    if(argc != 2)
    {
       printf("ex: spp-test 00:11:22:33:44:55\n");
       return 0;
    }
    
    if(strlen(argv[1])==17){
        strcpy(dest, argv[1]);
    }else{
        printf("mac length error\n");
        return 0;
    }

    
    
    // allocate a socket
    rfcommsock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

      
    // set the connection parameters (who to connect to)
    addr.rc_family = AF_BLUETOOTH;

    
    //printf("113\n");
    // 先找到 SPP Server 可被連接的 Port Number ( or channel number )
    addr.rc_channel = get_rfcomm_port_number(dest);
    
    //printf("116\n");
    
    str2ba( dest, &addr.rc_bdaddr );
    
    //printf("118\n");

    //  等幾秒鐘之後再連接到 SPP Server
    sleep(1);
    // 連接 SPP Server
    status = connect(rfcommsock, (struct sockaddr *)&addr, sizeof(addr));

//------------------------------------------------------------------------------
    // send/receive messages
    if( status == 0 ) {
        
        // say hello to client side
        status = send(rfcommsock, "hello!\n", 7, 0);
        if( status < 0 )
        {
            perror( "rfcomm send " );
            close(rfcommsock);
            return -1;
        }
        
        while(1)
        {
            // 從 RFCOMM socket 讀取資料
            // 這個 socket 
            // this socket has blocking turned off so it will never block,
            // even if no data is available

            len = recv(rfcommsock, rfcommbuffer, 255, 0);
  
            //printf("len=%d\n ",len);
            
    		// EWOULDBLOCK indicates the socket would block if we had a
    		// blocking socket.  we'll safely continue if we receive that
    		// error.  treat all other errors as fatal
    		if (len < 0 && errno != EWOULDBLOCK)
    		{
    			printf("rfcomm recv ");
    			break;
    		}
    		else if (len > 0)
    		{
    			// received a message; print it to the screen and
    			// return ATOK to the remote device
    			rfcommbuffer[len] = '\0';
                        i++;
    
                //printf("rfcomm received: %s\n", rfcommbuffer);
                printf("%s", rfcommbuffer);
                //status = send(rfcommsock, "ATOK\r\n", 6, 0);
                //if( status < 0 )
                //{
                //    perror("rfcomm send ");
                //    break;
                //}
            }

        }
    }
    else if( status < 0 ) 
    {
        perror("uh oh");
    }

    close(rfcommsock);
    return 0;
}
