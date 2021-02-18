#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <bluetooth/rfcomm.h>
#include <sys/wait.h>
#include <pthread.h>
#include <signal.h>
#include <opencv2/opencv.hpp>
#include <iostream>

using namespace std;
using namespace cv;

int writepoint[2]; // loc(x,y)
Point fix;
int fix_value_stack=0;
Point left_top;
Point left_bottom;
Point right_top;
Point right_bottom;
bool finish_flag = false;

Point getContourCenter(const Mat& mask) {

    Mat dst;

    distanceTransform(mask, dst, CV_DIST_L2, 5); // mask

    int maxIdx[2];

    minMaxIdx(dst, NULL, NULL, NULL, maxIdx, mask); //

    return Point(maxIdx[1], maxIdx[0]);

}  //////code-touch*/

void on_mouse(int event, int x, int y, int flags, void*)
{
    
    switch(event){
        case EVENT_LBUTTONDOWN:
            switch(fix_value_stack){
                case 0:
                    left_top=Point(x, y);
                    fix_value_stack++;
                    break;
                case 1:
                    left_bottom=Point(x, y);
                    fix_value_stack++;
                    break;
                case 2: 
                    right_top=Point(x, y);
                    fix_value_stack++;
                    break;
                case 3:
                    right_bottom=Point(x, y);
                    fix_value_stack++;
                    break;
                default:
                    printf("already done\n");
                    finish_flag = true;
                    break;
                
                
            }
            
            cout<<"click"<<fix_value_stack-1<<", "<<x<<", "<<y<<endl;
            break;
        case EVENT_LBUTTONUP:
            cout<<"EVENT_LBUTTONUP: "<<x<<", "<<y<<endl;
            break;
        case EVENT_MOUSEMOVE:
            if(flags & EVENT_FLAG_LBUTTON){
                //line(img_frame, fix, Point(x,y), Scalar(0,255,255), 2);
           //     imshow("img", img_input);
                fix = Point(x,y);
            }
            break;
        default:
            break;
    }
}

void *ThreadMain(void *argument);
bdaddr_t bdaddr_any = {0, 0, 0, 0, 0, 0};
bdaddr_t bdaddr_local = {0, 0, 0, 0xff, 0xff, 0xff};

int _str2uuid( const char *uuid_str, uuid_t *uuid ) {
    /* This is from the pybluez stack */

    uint32_t uuid_int[4];
    char *endptr;

    if( strlen( uuid_str ) == 36 ) {
        char buf[9] = { 0 };

        if( uuid_str[8] != '-' && uuid_str[13] != '-' &&
        uuid_str[18] != '-' && uuid_str[23] != '-' ) {
        return 0;
    }
    // first 8-bytes
    strncpy(buf, uuid_str, 8);
    uuid_int[0] = htonl( strtoul( buf, &endptr, 16 ) );
    if( endptr != buf + 8 ) return 0;
        // second 8-bytes
        strncpy(buf, uuid_str+9, 4);
        strncpy(buf+4, uuid_str+14, 4);
        uuid_int[1] = htonl( strtoul( buf, &endptr, 16 ) );
        if( endptr != buf + 8 ) return 0;

        // third 8-bytes
        strncpy(buf, uuid_str+19, 4);
        strncpy(buf+4, uuid_str+24, 4);
        uuid_int[2] = htonl( strtoul( buf, &endptr, 16 ) );
        if( endptr != buf + 8 ) return 0;

        // fourth 8-bytes
        strncpy(buf, uuid_str+28, 8);
        uuid_int[3] = htonl( strtoul( buf, &endptr, 16 ) );
        if( endptr != buf + 8 ) return 0;

        if( uuid != NULL ) sdp_uuid128_create( uuid, uuid_int );
    } else if ( strlen( uuid_str ) == 8 ) {
        // 32-bit reserved UUID
        uint32_t i = strtoul( uuid_str, &endptr, 16 );
        if( endptr != uuid_str + 8 ) return 0;
        if( uuid != NULL ) sdp_uuid32_create( uuid, i );
    } else if( strlen( uuid_str ) == 4 ) {
        // 16-bit reserved UUID
        int i = strtol( uuid_str, &endptr, 16 );
        if( endptr != uuid_str + 4 ) return 0;
        if( uuid != NULL ) sdp_uuid16_create( uuid, i );
    } else {
        return 0;
    }

    return 1;

}

//// uuid identify

sdp_session_t *register_service(uint8_t rfcomm_channel) {

    
    const char *service_name = "Armatus Bluetooth server";
    const char *svc_dsc = "A HERMIT server that interfaces with the Armatus Android app";
    const char *service_prov = "Armatus";

    uuid_t root_uuid, l2cap_uuid, rfcomm_uuid, svc_uuid,
           svc_class_uuid;
    sdp_list_t *l2cap_list = 0,
                *rfcomm_list = 0,
                 *root_list = 0,
                  *proto_list = 0,
                   *access_proto_list = 0,
                    *svc_class_list = 0,
                     *profile_list = 0;
    sdp_data_t *channel = 0;
    sdp_profile_desc_t profile;
    sdp_record_t record = { 0 };
    sdp_session_t *session = 0;

    // set the general service ID
    //sdp_uuid128_create(&svc_uuid, &svc_uuid_int);
    _str2uuid("00001101-0000-1000-8000-00805F9B34FB",&svc_uuid);
    sdp_set_service_id(&record, svc_uuid);

    char str[256] = "";
    sdp_uuid2strn(&svc_uuid, str, 256);
    printf("Registering UUID %s\n", str);

    // set the service class
    sdp_uuid16_create(&svc_class_uuid, SERIAL_PORT_SVCLASS_ID);
    svc_class_list = sdp_list_append(0, &svc_class_uuid);
    sdp_set_service_classes(&record, svc_class_list);

    // set the Bluetooth profile information
    sdp_uuid16_create(&profile.uuid, SERIAL_PORT_PROFILE_ID);
    profile.version = 0x0100;
    profile_list = sdp_list_append(0, &profile);
    sdp_set_profile_descs(&record, profile_list);

    // make the service record publicly browsable
    sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
    root_list = sdp_list_append(0, &root_uuid);
    sdp_set_browse_groups(&record, root_list);

    // set l2cap information
    sdp_uuid16_create(&l2cap_uuid, L2CAP_UUID);
    l2cap_list = sdp_list_append(0, &l2cap_uuid);
    proto_list = sdp_list_append(0, l2cap_list);

    // register the RFCOMM channel for RFCOMM sockets
    sdp_uuid16_create(&rfcomm_uuid, RFCOMM_UUID);
    channel = sdp_data_alloc(SDP_UINT8, &rfcomm_channel);
    rfcomm_list = sdp_list_append(0, &rfcomm_uuid);
    sdp_list_append(rfcomm_list, channel);
    sdp_list_append(proto_list, rfcomm_list);

    access_proto_list = sdp_list_append(0, proto_list);
    sdp_set_access_protos(&record, access_proto_list);

    // set the name, provider, and description
    sdp_set_info_attr(&record, service_name, service_prov, svc_dsc);

    // connect to the local SDP server, register the service record,
    // and disconnect
    session = sdp_connect(&bdaddr_any, &bdaddr_local, SDP_RETRY_IF_BUSY);
    sdp_record_register(session, &record, 0);

    // cleanup
    sdp_data_free(channel);
    sdp_list_free(l2cap_list, 0);
    sdp_list_free(rfcomm_list, 0);
    sdp_list_free(root_list, 0);
    sdp_list_free(access_proto_list, 0);
    sdp_list_free(svc_class_list, 0);
    sdp_list_free(profile_list, 0);

    return session;
}

//// server


/*
 * 
char input[1024] = { 0 };

char *read_server(int client) {
    // read data from the client
    int bytes_read;
    bytes_read = read(client, input, sizeof(input));
    if (bytes_read > 0) {
        printf("received [%s]\n", input);
        return input;
    } else {
        return NULL;
    }
}
*/
 
void write_server(int client, char *message) {
    // send data to the client
    char messageArr[1024] = { 0 };
    int bytes_sent;
    strcpy(messageArr, message);

    bytes_sent = write(client, messageArr, strlen(messageArr));
    if (bytes_sent > 0) 
        printf("sent [%s] %d\n", messageArr, bytes_sent);
    
}

int main()
{

    pthread_t thread_id;  
  
    signal( SIGPIPE, SIG_IGN );  
    
    int port = 3, result, sock, client, bytes_read, bytes_sent;
    struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
    char buffer[1024] = { 0 };
    socklen_t opt = sizeof(rem_addr);

    // local bluetooth adapter
    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = bdaddr_any;
    loc_addr.rc_channel = (uint8_t) port;

    // register service
    sdp_session_t *session = register_service(port);
    // allocate socket
    sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    printf("socket() returned %d\n", sock);

    // bind socket to port 3 of the first available
    result = bind(sock, (struct sockaddr *)&loc_addr, sizeof(loc_addr));
    printf("bind() on channel %d returned %d\n", port, result);

    // put socket into listening mode
    result = listen(sock, 1);
    printf("listen() returned %d\n", result);

    //sdpRegisterL2cap(port);
    
   while(1)
   {
        // accept one connection
        printf("calling accept()\n");
        client = accept(sock, (struct sockaddr *)&rem_addr, &opt);
        printf("accept() returned %d\n", client);
    
        ba2str(&rem_addr.rc_bdaddr, buffer);
        fprintf(stderr, "accepted connection from %s\n", buffer);
        memset(buffer, 0, sizeof(buffer));
        
        pthread_create( &thread_id, NULL, ThreadMain, (void*)client);   
    }

}


void *ThreadMain(void *argument)  
{  
    
    char buff[1024] = "";
    char buff2[1024] = "";
    pthread_detach(pthread_self());  
    int client = (int)argument;  
    Scalar all(255, 255, 255); 
    ///// cv part-start
    VideoCapture cap(-1);
    
    //VideoCapture cap("09_22_test.avi");
    Mat img_frame,re_frame, img_hsv, img_arr,img_mask;
    Mat rgb_color = Mat(1, 1, CV_8UC3, all); //back
    Mat hsv_color; //back
    Mat img_mask2;
    
    cv::cvtColor(rgb_color, hsv_color, cv::COLOR_RGB2HSV);
    
    if (!cap.isOpened()) {
        cerr << "Error! Unable to open camera\n";
        exit(0);
    }

    int LowH = 120;
    int HighH = 179;
    int LowS = 90;
    int HighS = 255;
    int LowV = 60;
    int HighV = 255;

  

    namedWindow("color range set");
    createTrackbar("Lower H", "color range set", &LowH, 179);
	createTrackbar("Upper H", "color range set", &HighH, 179);	
    createTrackbar("Lower S", "color range set", &LowS, 255);
	createTrackbar("Upper S", "color range set", &HighS, 255);	
    createTrackbar("Lower V", "color range set", &LowV, 255);
	createTrackbar("Upper V", "color range set", &HighV, 255);	
    // recognition color


    int BLowH = 0;
    int BHighH = 179;
    int BLowS = 0;
    int BHighS = 255;
    int BLowV = 100;
    int BHighV = 255;
    int count =0;
    
    int left =0;
    int top=0;
    int width=0; 
    int height=0;
    
    int leftfix= 40;
    int topfix = 75;
    int widthfix = 360;
    int heightfix = 200;
    
    int left_result=0;
    int top_result=0;
    int width_result=0;
    int height_result=0;
        
    
    
    
    /*
    namedWindow("range set");    
    createTrackbar("Lower H", "Bcolor range set", &BLowH, 179);
	createTrackbar("Upper H", "Bcolor range set", &BHighH, 179);	
    createTrackbar("Lower S", "Bcolor range set", &BLowS, 255);
	createTrackbar("Upper S", "Bcolor range set", &BHighS, 255);	
    createTrackbar("Lower V", "Bcolor range set", &BLowV, 255);
	createTrackbar("Upper V", "Bcolor range set", &BHighV, 255);
    */

    for (;;)
    {
       
        cap.read(img_frame);
        //imshow("frame", img_frame);
    
      if (img_frame.empty()) {
            cerr << "ERROR! blank frame grabbled\n";
            break;
        }

        cv::cvtColor(img_frame, img_hsv, cv::COLOR_RGB2HSV);
        // color translate
        inRange(img_hsv, Scalar(LowH, LowS, LowV), Scalar(HighH, HighS, HighV), img_mask);
        // 0/1 image
       
        erode(img_mask, img_mask, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
        dilate(img_mask, img_mask, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
        dilate(img_mask, img_mask, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
        erode(img_mask, img_mask, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
        //erode-dilate calculate

    

        if(count==0){ // background contouring
        cv::cvtColor(img_frame, hsv_color, cv::COLOR_RGB2HSV);
        inRange(hsv_color, Scalar(BLowH, BLowS, BLowV), Scalar(BHighH, BHighS, BHighV), img_mask2);

        erode(img_mask2, img_mask2, getStructuringElement(MORPH_ELLIPSE, Size(3, 3)));
        dilate(img_mask2, img_mask2, getStructuringElement(MORPH_ELLIPSE, Size(3, 3)));
        dilate(img_mask2, img_mask2, getStructuringElement(MORPH_ELLIPSE, Size(3, 3)));
        erode(img_mask2, img_mask2, getStructuringElement(MORPH_ELLIPSE, Size(3, 3)));


        Mat img_labels, stats, centroids;
        int numOfLables = connectedComponentsWithStats(img_mask2, img_labels, stats, centroids, 8, CV_32S);
        //라벨링 , stats- labeling image index, cetroid - labeling image center location
               
        int max = -1, idx = 0;
        for (int j = 1; j < numOfLables; j++) {
            int area = stats.at<int>(j, CC_STAT_AREA);
            if (max < area)
            {
                max = area;
                idx = j;
            }
        }
        //영역박스 표시    
        
        
        
        
        
        left = stats.at<int>(idx, CC_STAT_LEFT);
        top = stats.at<int>(idx, CC_STAT_TOP);
        width = stats.at<int>(idx, CC_STAT_WIDTH);
        height = stats.at<int>(idx, CC_STAT_HEIGHT);
        
        
  /*       //좌표 저장
        Point leftT(left,top);        
        Point leftB(left,top+height);
        Point RightT(left+width,top);
        Point RightB(left+width,top+height); 
  */    
        int left_marginoferror = fabs(leftfix-left);
        int top_marginoferror = fabs(topfix-top);
        int width_marginoferror = fabs(widthfix-width);
        int height_marginoferror = fabs(heightfix-height);
        
      
        
        if(left_marginoferror <30 && top_marginoferror <30 && width_marginoferror <30 && height_marginoferror <30)
        {
            
           left_result= left;
           top_result= top;
           width_result= width;
            height_result= height;
            printf("fix location");
            
            
        }
        
        else 
        {
            
            left_result= left;
            top_result= top;
            width_result= width;
            height_result= height;
            printf("free location\n");
            
            
            /*
            left_result= leftfix;
            top_result= topfix;
            width_result= widthfix;
            height_result= heightfix;
            fix
            */
            
            /*
            namedWindow("frame");
            
            while(1){
                
                if(!finish_flag){
                    setMouseCallback("frame", on_mouse);
                }
                else break;
            }
            left_result= left_top.x;
            top_result= left_top.y;
            width_result= right_top.x-left_top.x;
            height_result= left_bottom.y - left_top.y; 
            printf("mouse fix is finished\n"); 
            */
        }    

	if(width_result>300 && width_result<620 && height_result>180) { 
         rectangle( img_frame, Point(left_result,top_result), Point(left_result+width_result,top_result+height_result),Scalar(0,0,255),1 ); 
         printf("\n %d %d %d %d\n",left_result,top_result,width_result,height_result);
         
         count++;
         }
        
    }
        if(count==1){
            rectangle( img_frame, Point(left_result,top_result), Point(left_result+width_result,top_result+height_result),Scalar(0,0,255),1 ); 
            imshow("correct",img_mask2);
        }
        
        vector<vector<Point> > contours;
        findContours(img_mask.clone(), contours, RETR_LIST, CHAIN_APPROX_SIMPLE);
        // find contour
        
        vector<Point2f> approx;
        //apporx contour   

        Point center;
        
        Rect num[1000]; // contour num
        
        
        if(contours.size()==0){
            sprintf(buff2, "%c\n", 'N');
            write_server(client, buff2); 
            continue;
        }
        for (size_t i = 0; i < contours.size(); i++)
        {
            approxPolyDP(Mat(contours[i]), approx, arcLength(Mat(contours[i]), true) * 0.02, true);
            Rect ans(0, 0, 0, 0);
            
                if ( fabs(contourArea(Mat(approx))) > 5 && fabs(contourArea(Mat(approx))) < 300)
                {
                    center = getContourCenter(img_mask);
                /*
                Point center2;
                center2.x = center.x - left;
                center2.y = center.y - top;
                */
                    /*
                    if(center.x < left-10 && center.x > left + width+10 && center.y < top-10 && center.y > top + height+10){
                        printf("its over\n");
                        continue;
                    }
                    */
                    Rect test(center.x - 7, center.y - 7, 14, 14);
                    num[i] = test;

                    if(i>1)
                        ans = num[i-1] & num[i];

                    if (ans.area() <= 0)
                    {                
                        writepoint[0]=1980/width*(center.x-left_result);
                        writepoint[1]=1080/height*(center.y-top_result);
            
                        
                        sprintf (buff, "%d %d\n", writepoint[0],writepoint[1]);  
                        write_server(client, buff);

                    circle(img_mask, center, 2, Scalar(0, 255, 0), -1);
                    circle(img_frame, center, 1, Scalar(255, 0, 0), -1);
                    rectangle(img_mask, test, Scalar(0, 255, 0), 2);
                    rectangle(img_frame, test, Scalar(0, 0, 255), 2);
                    }

            }
           
                        
           
        }
           
           
           
        
       
         imshow("mask", img_mask);
     //   imshow("mask_back",img_mask2);
 
      imshow("frame", img_frame);
        
       if (waitKey(10)==27){
		break;
        }
        
    }
    ///// cv part-end
    printf("disconnected\n" );  
    close(client);  
  
    return 0;
}  
