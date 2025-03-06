#include <osgDB/ReadFile>
#include <osgUtil/Optimizer>
#include <osgViewer/Viewer>
#include "testNode.h"

#include <osgViewer/ViewerEventHandlers>
#include <osgViewer/ImGuiHandler>
#include <osgGA/StateSetManipulator>
#include <osgGA/TrackballManipulator>

#include <iostream>

int main(int argc, char** argv)
{
	osgViewer::Viewer viewer;
	viewer.addEventHandler(new osgViewer::StatsHandler);
	viewer.addEventHandler(new osgGA::StateSetManipulator(viewer.getCamera()->getOrCreateStateSet()));
	viewer.addEventHandler(new osgViewer::ToggleSyncToVBlankHandler);
	viewer.addEventHandler(new osgViewer::ImGuiHandler);
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
		viewer.getCamera()->setClearColor({ 0, 0, 0, 1 });
		viewer.setCameraManipulator(new osgGA::TrackballManipulator);
		viewer.getCameraManipulator()->setHomePosition({ 0, 0, 3 }, { 0, 0, 0 }, { 0, 1, 0 });
    }

	auto grp = new Group;
	auto node = new TestNode;
	grp->addChild(node);
	viewer.setSceneData(grp);

    return viewer.run();
}
