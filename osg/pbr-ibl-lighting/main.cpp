#include <osgDB/ReadFile>
#include <osgUtil/Optimizer>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>
#include "testNode.h"


#include <iostream>

using namespace osg;

int main(int argc, char** argv)
{
	Quat q(90, Vec3d(0, 0, 1));
	Vec3 xx(1, 0, 0);
	auto yy = q * xx;

	//osg::setNotifyLevel(DEBUG_INFO);
    osgViewer::Viewer viewer;
    viewer.setThreadingModel(viewer.SingleThreaded);
	//viewer.setLightingMode(viewer.NO_LIGHT);
	{
        osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;

        traits->x = 40;
        traits->y = 40;
        traits->width = 600;
        traits->height = 480;
        traits->windowDecoration = true;
        traits->doubleBuffer = true;
        traits->sharedContext = 0;
        traits->readDISPLAY();
        traits->setUndefinedScreenDetailsToDefaultScreen();

        osg::ref_ptr<osg::GraphicsContext> gc = osg::GraphicsContext::createGraphicsContext(traits.get());

        osg::ref_ptr<osg::Camera> camera = new osg::Camera;
		
        camera->setGraphicsContext(gc.get());
        camera->setViewport(new osg::Viewport(0, 0, traits->width, traits->height));
		camera->setComputeNearFarMode(camera->DO_NOT_COMPUTE_NEAR_FAR);
        GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
        camera->setDrawBuffer(buffer);
        camera->setReadBuffer(buffer);
		viewer.addSlave(camera);
    }
	viewer.addEventHandler(new osgViewer::StatsHandler);

	auto grp = new Group;
	auto node = new TestNode;
	grp->addChild(node);
	viewer.setSceneData(grp);

    return viewer.run();
}
