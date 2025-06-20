#include <osgDB/ReadFile>
#include <osgUtil/Optimizer>
#include <osgViewer/Viewer>
#include "LumNode.h"

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
  {
    auto handler = new osgViewer::ImGuiHandler;
    handler->setFont();
    viewer.addEventHandler(handler);
  }
  viewer.setThreadingModel(viewer.SingleThreaded);
  // viewer.setLightingMode(viewer.NO_LIGHT);
  {
    osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;

    traits->x = 40;
    traits->y = 40;
    traits->width = 1200;
    traits->height = 900;
    traits->windowDecoration = true;
    traits->doubleBuffer = true;
    traits->sharedContext = 0;
    traits->red = 8;
    traits->green = 8;
    traits->blue = 8;
    traits->alpha = 8;
    traits->readDISPLAY();
    traits->setUndefinedScreenDetailsToDefaultScreen();

    osg::ref_ptr<osg::GraphicsContext> gc = osg::GraphicsContext::createGraphicsContext(traits.get());


    auto camera = viewer.getCamera();
    camera->setGraphicsContext(gc.get());
    camera->setViewport(new osg::Viewport(0, 0, traits->width, traits->height));
    //camera->setComputeNearFarMode(camera->DO_NOT_COMPUTE_NEAR_FAR);
    camera->setClearColor({0, 0, 0, 1});
  }

  auto node = new LumNode;
  viewer.setSceneData(node);

  return viewer.run();
}
