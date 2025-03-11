#include "testNode.h"
#include <osgDB/ReadFile>
#include <osgUtil/Optimizer>
#include <osgViewer/Viewer>

#include <osgGA/StateSetManipulator>
#include <osgGA/TrackballManipulator>
#include <osgViewer/ImGuiHandler>
#include <osgViewer/ViewerEventHandlers>

#include <iostream>

int main(int argc, char **argv)
{
  osgViewer::Viewer viewer;
  viewer.addEventHandler(new osgViewer::StatsHandler);
  viewer.addEventHandler(new osgGA::StateSetManipulator(viewer.getCamera()->getOrCreateStateSet()));
  viewer.addEventHandler(new osgViewer::ToggleSyncToVBlankHandler);
  viewer.addEventHandler(new osgViewer::ImGuiHandler);
  viewer.setThreadingModel(viewer.SingleThreaded);
  viewer.setLightingMode(viewer.NO_LIGHT);
  viewer.getCamera()->setClearColor({0, 0, 0, 1});
  viewer.realize();

  auto node = new TestNode;
  viewer.setSceneData(node);

  {
    auto manip = new osgGA::TrackballManipulator();
    manip->setVerticalAxisFixed(true);
    viewer.setCameraManipulator(manip);
  }

  return viewer.run();
}
