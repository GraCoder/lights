#include <osgDB/ReadFile>
#include <osgUtil/Optimizer>
#include <osgViewer/Viewer>
#include <osg/Matrixd>
#include "testNode.h"


#include <iostream>

using namespace osg;

int main(int argc, char** argv)
{
	//Matrix m;
	//m.makeLookAt({100, 0, 200}, {0, 0, 100}, {0, 1, 0});
	//Matrix m1 = m;

	//m1.setTrans(0, 0, 0);
	//m1.preMult(osg::Matrix::translate(0, 0, -100));
	//m1.postMult(osg::Matrix::translate(0, 0, -141.42135623730950488016887242097));


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
        gc->getState()->setCheckForGLErrors(osg::State::ONCE_PER_ATTRIBUTE);

        osg::ref_ptr<osg::Camera> camera = new osg::Camera;
		
        camera->setGraphicsContext(gc.get());
        camera->setViewport(new osg::Viewport(0, 0, traits->width, traits->height));
		camera->setComputeNearFarMode(camera->DO_NOT_COMPUTE_NEAR_FAR);
        GLenum buffer = traits->doubleBuffer ? GL_BACK : GL_FRONT;
        camera->setDrawBuffer(buffer);
        camera->setReadBuffer(buffer);
		viewer.addSlave(camera);
    }

	auto grp = new Group;
	auto node = new TestNode;
	grp->addChild(node);
	viewer.setSceneData(grp);

    return viewer.run();
}
