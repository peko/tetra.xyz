#include "ofApp.h"



//--------------------------------------------------------------

void ofApp::setup() {
    
    gui0 = new ofxUISuperCanvas("Controls");

    proj_lng = 90.0;
    proj_lat = 90.0;

    gui0->addSpacer();
    gui0->addLabel("Projection point");
    gui0->addSlider("Lng", -180.0, 180.0, &proj_lng);
    gui0->addSlider("Lat",  -90.0,  90.0, &proj_lat);
    gui0->autoSizeToFitWidgets();
    ofAddListener(gui0->newGUIEvent,this,&ofApp::guiEvent);

    ofSetVerticalSync(true);
    ofSetDepthTest(true);

    setProjection();
    parseShapeFile("data/ne_110m_admin_0_countries");
    reprojectShape();

    sphere.set(200.0, 3);

}

//--------------------------------------------------------------

void ofApp::parseShapeFile(string fname) {

    // shape file
    bValidate     = 0;
    nInvalidCount = 0;
    bHeaderOnly   = 0;
    
    // open shape
    hSHP = SHPOpen( fname.c_str(), "rb");
    if( hSHP == NULL ) {
        printf( "Unable to open Shape:%s\n", fname.c_str() );
        ofExit(1);
    }

    // open dbf
    hDBF = DBFOpen( fname.c_str(), "rb");
    if( hDBF == NULL ) {
        printf( "Unable to open DBF:%s\n", fname.c_str() );
        ofExit(1);
    }

    printf("DBF records: %d\n", DBFGetRecordCount(hDBF));
    printf("DBF fields: %d\n", DBFGetFieldCount(hDBF));
    

    SHPGetInfo( hSHP, &nEntities, &nShapeType, adfMinBound, adfMaxBound );

    printf( "Shapefile Type: %s   # of Shapes: %d\n\n",
            SHPTypeName( nShapeType ), nEntities );

    printf( "File Bounds: (%.15g,%.15g,%.15g,%.15g)\n"
            "         to  (%.15g,%.15g,%.15g,%.15g)\n",
            adfMinBound[0],
            adfMinBound[1],
            adfMinBound[2],
            adfMinBound[3],
            adfMaxBound[0],
            adfMaxBound[1], 
            adfMaxBound[2],
            adfMaxBound[3] );

    mbrXmin = adfMinBound[0];
    mbrYmin = adfMinBound[1];
    mbrXmax = adfMaxBound[0];
    mbrYmax = adfMaxBound[1];

    // shapes
    for( i = 0; i < nEntities && !bHeaderOnly; i++ ) {

        int  j;
        SHPObject *shp;

        shp = SHPReadObject( hSHP, i );

        if( shp == NULL ) {
            fprintf( stderr, "Unable to read shape %d, terminating object reading.\n", i );
            break;
        }


        if( shp->nParts > 0 && shp->panPartStart[0] != 0 ) {
            fprintf( stderr, "panPartStart[0] = %d, not zero as expected.\n",
                     shp->panPartStart[0] );
        }


        vector<geo> gshape;
        int oldiPart=-1;
        // points
        for( j = 0, iPart = 1; j < shp->nVertices; j++ ) {

            const char  *pszPartType = "";

            if( j == 0 && shp->nParts > 0 )
                pszPartType = SHPPartTypeName( shp->panPartType[0] );

            if( iPart < shp->nParts
                && shp->panPartStart[iPart] == j ) {
                pszPartType = SHPPartTypeName( shp->panPartType[iPart] );
                iPart++;
                pszPlus = "+";
            }
            else pszPlus = " ";

            if( shp->bMeasureIsUsed ) {
                /*
                printf("   %s (%.15g,%.15g, %.15g, %.15g) %s \n",
                       pszPlus,
                       shp->padfX[j],
                       shp->padfY[j],
                       shp->padfZ[j],
                       shp->padfM[j],
                       pszPartType );
                */
            } else {
                /*
                printf("   %s (%.15g,%.15g, %.15g) %s \n",
                       pszPlus,
                       shp->padfX[j],
                       shp->padfY[j],
                       shp->padfZ[j],
                       pszPartType );
                */
                // float R = 200.0;
                double lng = shp->padfX[j]*DEG_TO_RAD;
                double lat = shp->padfY[j]*DEG_TO_RAD;
                // float x = R*cos(lat)*cos(lng);
                // float y = R*cos(lat)*sin(lng);
                // float z = R*sin(lat);

 

                // int p = pj_transform(pjFrom, pjTo, 1, 1, &lng, &lat, NULL);

                if(mbrXmin>lng) mbrXmin = lng;
                if(mbrYmin>lat) mbrYmin = lat;
                if(mbrXmax<lng) mbrXmax = lng;
                if(mbrYmax<lat) mbrYmax = lat;

                // if(p)
                //     printf("%s\nlng: %f, lat: %f\n",pj_strerrno(p), lng, lat);
                //else
                //    printf("lng: %f, lat: %f\n", lng, lat);

                if(oldiPart == iPart) {
                    // path.lineTo(x,y,z);
                    // projectedPath.lineTo(lng, lat);

                    geo g = {lng, lat}; 
                    gshape.push_back(g);

                } else {
                    
                    // path.moveTo(x,y,z);
                    // projectedPath.moveTo(lng, lat);
                    
                    // break;

                    gshapes.push_back(gshape);
                    gshape.clear();
                    
                    geo g = {lng, lat}; 
                    gshape.push_back(g);

                }

                oldiPart = iPart;
            }
        }

        gshapes.push_back(gshape);

        if( bValidate ) {
            int nAltered = SHPRewindObject( hSHP, shp );

            if( nAltered > 0 )
            {
                printf( "  %d rings wound in the wrong direction.\n",
                        nAltered );
                nInvalidCount++;
            }
        }

        // path.close();
        // projectedPath.close();

        // shapes.push_back(path);
        // projectedShapes.push_back(projectedPath);

        SHPDestroyObject( shp );
    }

    printf("MBR\n   min: %f, %f;\n   max: %f, %f\n", mbrXmin, mbrYmin, mbrXmax, mbrYmax);

    SHPClose( hSHP );
    DBFClose( hDBF);

    if( bValidate ) {
        printf( "%d object has invalid ring orderings.\n", nInvalidCount );
    }

    #ifdef USE_DBMALLOC
        malloc_dump(2);
    #endif
}

//--------------------------------------------------------------
void ofApp::setProjection() {

    char buf[256];

    pjFromStr = "+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs";
    sprintf(buf, "+proj=laea +lat_0=%f +lon_0=%f +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs", proj_lat, proj_lng);
    pjToStr  = (string) buf;

    // projections
    if (!(pjFrom = pj_init_plus(pjFromStr.c_str())) )
       ofExit(1);
    if (!(pjTo = pj_init_plus(pjToStr.c_str())) )
       ofExit(1);
}

void ofApp::reprojectShape() {

    setProjection();
    // printf("%d\n",gshapes.size() );
    projectedShapes.clear();
    for(vector< vector<geo> >::iterator gs = gshapes.begin(); gs != gshapes.end(); ++gs) {
        
        ofPath projectedPath;

        projectedPath.setFilled(false);
        projectedPath.setStrokeWidth(1);

        for(vector<geo>::iterator gp = (*gs).begin(); gp!= (*gs).end(); ++gp) {
            double lat = (*gp).lat;
            double lng = (*gp).lng;

            pj_transform(pjFrom, pjTo, 1, 1, &lng, &lat, NULL);
            
            if(mbrXmin>lng) mbrXmin = lng;
            if(mbrYmin>lat) mbrYmin = lat;
            if(mbrXmax<lng) mbrXmax = lng;
            if(mbrYmax<lat) mbrYmax = lat;

            if(gp == (*gs).begin())
                projectedPath.moveTo(lng, lat);
            else  
                projectedPath.lineTo(lng, lat);
        }
        projectedPath.close();
        projectedShapes.push_back(projectedPath);
    }

}

//--------------------------------------------------------------

void ofApp::exit() {
    delete gui0;
}

//--------------------------------------------------------------
void ofApp::update() {

}

//--------------------------------------------------------------
void ofApp::draw() {

    ofBackgroundGradient(ofColor(64), ofColor(0));

    ofSetColor(255);

    // ofDrawBitmapString(pjFromStr,10,20);
    // ofDrawBitmapString(pjToStr  ,10,40);

    // gui0->draw();

    double s=-768.0/abs(mbrYmax-mbrYmin);
    
    ofPushMatrix();

        ofTranslate(1024/2, 768/2, 0);
        ofScale(-s,s,1.0);

        for(vector< ofPath >::iterator shape = projectedShapes.begin(); shape != projectedShapes.end(); ++shape) {
            (*shape).draw();
        }

        // for(vector< ofPath* >::iterator shape = shapes.begin(); shape != shapes.end(); ++shape) {
        //  (*shape)->draw();
        // }

        cam.begin();

        ofSetColor(255,255,255,20);
        sphere.drawWireframe();
        //mesh.drawWireframe();

        //glPointSize(2);
        //ofSetColor(ofColor::white);
        //mesh.drawVertices();

        //for(vector< ofPath* >::iterator shape = shapes.begin(); shape != shapes.end(); ++shape) {
        //  (*shape)->draw();
        //}
        cam.end();
    ofPopMatrix();        


    /*
    int n = mesh.getNumVertices();
    float nearestDistance = 0;
    ofVec2f nearestVertex;
    int nearestIndex = 0;
    ofVec2f mouse(mouseX, mouseY);
    for(int i = 0; i < n; i++)
    {
        ofVec3f cur = cam.worldToScreen(mesh.getVertex(i));
        float distance = cur.distance(mouse);
        if(i == 0 || distance < nearestDistance)
        {
            nearestDistance = distance;
            nearestVertex = cur;
            nearestIndex = i;
        }
    }

    ofSetColor(ofColor::gray);
    ofLine(nearestVertex, mouse);

    ofNoFill();
    ofSetColor(ofColor::yellow);
    ofSetLineWidth(2);
    ofCircle(nearestVertex, 4);
    ofSetLineWidth(1);

    ofVec2f offset(10, -10);
    ofDrawBitmapStringHighlight(ofToString(nearestIndex), mouse + offset);
    */

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {

    switch (key)  {
        // case 'p': {
        //     drawPadding = !drawPadding; 
        //     gui0->setDrawWidgetPadding(drawPadding);          
        //     }
        //     break;
            
        case 'g': {
            gui0->toggleVisible(); 
            }
            break; 
        default:
            break;
    }
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key) {

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y) {

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button) {

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button) {

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h) {

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg) {

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo) {

}

//--------------------------------------------------------------
void ofApp::guiEvent(ofxUIEventArgs &e) {
    // printf("UI Event\n");
    reprojectShape();
}