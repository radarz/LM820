#include "display.h"

#pragma warning (disable:4819)
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <memory>
#include <map>
#include <cstdint>

using namespace cv;

#define MAX_WINDOW_NUM      (2)
#define SLIDER_SCALE  (0.005)
#define SLIDER_MAX    (80)
#define SLIDER_MIN    (8)

///private function/////////////////////////////////////////
static void trackbar_callback(int value, void *user_data);
static void mouse_callback(int event, int x, int y, int, void* user_data);

class DisplayContext {
 public:
  DisplayContext(std::string _name) {
    name = _name;
    slider_value = 10;
    namedWindow(name);
    createTrackbar("SCALE ", name, &slider_value, (SLIDER_MAX - SLIDER_MIN), trackbar_callback, this);
    setMouseCallback(name, mouse_callback, this);
    dbg_filename = nullptr;
    fps = 0;
    frm_count = 0;
    depth_fov = 60;
    fov_deviation = 0;
    show_extra_info = false;
  }

  ~DisplayContext() {
    destroyWindow(name);
  }

  std::string name;
  cv::Mat image;

  // only for depth display
  std::vector<uint16_t> depth_data;
  float depth_fov;
  float fov_deviation;

  int   slider_value;
  int   mouse_x, mouse_y;

  bool  show_extra_info;
  unsigned long frm_count;
  float fps;
  unsigned char dbg_progress;
  const char *  dbg_filename;
};

typedef std::map<std::string, std::shared_ptr<DisplayContext>> ContextMap;

static ContextMap data;
static TU32 g_nLastKey = 0;



//update depth image via orignal depth data & display scale
static void UpdateDepthImage(DisplayContext &ctx) {
  char buff[100];
  const int kWindowHeight = 600;
  const int kWindowWdith = 800;
  const float fov = (float)(ctx.depth_fov / 180 * CV_PI);
  const float fov_start = (float)((-ctx.depth_fov/2 + ctx.fov_deviation) / 180 * CV_PI);
  const float fov_end = (float)((ctx.depth_fov/2 + ctx.fov_deviation) / 180 * CV_PI);

  if (ctx.depth_data.empty()) {
    return;
  }
  ctx.image.create(kWindowHeight, kWindowWdith, CV_8UC3);
  ctx.image.setTo(0);
  float step = fov / ctx.depth_data.size();
  float scale = (float)((ctx.slider_value + SLIDER_MIN) * SLIDER_SCALE);
  //circle diagram
  for (int idx = 1; idx < 12; idx++) {
    float v = scale * idx * 1000;
    circle(ctx.image, cv::Point(kWindowWdith / 2, kWindowHeight), (int)v, cv::Scalar(0x00, 0x3f, 0x00));
  }
  float lx = kWindowWdith * sin(fov_end);
  float ly = kWindowWdith * cos(fov_end);
  cv::line(ctx.image, cv::Point(kWindowWdith / 2, kWindowHeight), cv::Point(kWindowWdith / 2 + (int)lx, kWindowHeight - (int)ly), cv::Scalar(0x0, 0xff, 0), 1);
  lx = kWindowWdith * sin(fov_start);
  ly = kWindowWdith * cos(fov_start);
  cv::line(ctx.image, cv::Point(kWindowWdith / 2, kWindowHeight), cv::Point(kWindowWdith / 2 + (int)lx, kWindowHeight - (int)ly), cv::Scalar(0x0, 0xff, 0), 1);
  //dots
  for (int idx = 0; idx < (int)ctx.depth_data.size(); idx++) {
    if (ctx.depth_data[idx] == 0) {
      continue;
    }
    float rou = fov_start + idx * step;
    float dx = sin(rou) * ctx.depth_data[idx] * scale;
    float dy = cos(rou) * ctx.depth_data[idx] * scale;
    int x = (int)(dx + kWindowWdith / 2);
    int y = (int)(kWindowHeight - dy);
    cv::circle(ctx.image, cv::Point(x, y), 1, cv::Scalar(0x00, 0x00, 0xff), -1);
  }
  //draw mouse position
  if (ctx.mouse_x > 0 && ctx.mouse_y > 0 && ctx.mouse_x < ctx.image.cols && ctx.mouse_y < ctx.image.rows) {
    int x = ctx.mouse_x - kWindowWdith / 2;
    int y = kWindowHeight - ctx.mouse_y;
    float v = atan(((float)x) / (float)y);
    int idx = (int)((v - fov_start) / fov * ctx.depth_data.size());
    if (idx >= 0 && idx < (int)ctx.depth_data.size()) { //display
      //compute line direction
      float dx = sin(v) * kWindowWdith + kWindowWdith / 2;
      float dy = kWindowHeight - cos(v) * kWindowWdith;
      cv::line(ctx.image, cv::Point(kWindowWdith / 2, kWindowHeight), cv::Point((int)dx, (int)dy), cv::Scalar(0xff, 0, 0), 1);
      int distance = ctx.depth_data[idx];
      //find point position
      float rou = fov_start + idx * step;
      dx = sin(rou) * distance * scale;
      dy = cos(rou) * distance * scale;
      int x = (int)(dx + kWindowWdith / 2);
      int y = (int)(kWindowHeight - dy);
      if (distance != 0) {
        //draw distance
        sprintf(buff, "%d", distance);
        cv::putText(ctx.image, buff, cv::Point(x, y - 20), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(0xff), 1);
        cv::circle(ctx.image, cv::Point(x, y), 4, cv::Scalar(0x00, 0xff, 0xff), 1);
      }
    }
  }
  // draw extra info
  if (ctx.show_extra_info) {
    // stat info
    sprintf(buff, "Frame count: %lu", ctx.frm_count);
    cv::putText(ctx.image, buff, cv::Point(0, 25), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(0xff), 1);
    sprintf(buff, "Frame rate: %.1f fps", ctx.fps);
    cv::putText(ctx.image, buff, cv::Point(0, 40), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(0xff), 1);
    // help info
    sprintf(buff, "Press the keys below to operate");
    cv::putText(ctx.image, buff, cv::Point(500, 25), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(0xff), 1);
    sprintf(buff, "    q - quit");
    cv::putText(ctx.image, buff, cv::Point(500, 40), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(0xff), 1);
    sprintf(buff, "    s - save debug image");
    cv::putText(ctx.image, buff, cv::Point(500, 55), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(0xff), 1);
    sprintf(buff, "    t - trig depth");
    cv::putText(ctx.image, buff, cv::Point(500, 70), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(0xff), 1);
    sprintf(buff, "    c - get continuous depth");
    cv::putText(ctx.image, buff, cv::Point(500, 85), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(0xff), 1);
    // debug image info
    if (ctx.dbg_filename) {
      sprintf(buff, "Reading debug image to %s...",  ctx.dbg_filename);
      cv::putText(ctx.image, buff, cv::Point(0, 55), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar::all(0xff), 1);
      sprintf(buff, "%d%%", ctx.dbg_progress);
      cv::putText(ctx.image, buff, cv::Point(100, 85), cv::FONT_HERSHEY_PLAIN, 2, cv::Scalar::all(0xff), 1);
    }
  }
}

//change scale event
static void trackbar_callback(int value, void *user_data) {
  if (!user_data) {
    return;
  }
  DisplayContext *pctx = (DisplayContext*)user_data;
  pctx->slider_value = value;
  UpdateDepthImage(*pctx);
  display_ShowImage(1);
}

static void mouse_callback(int event, int x, int y, int, void* user_data) {
  if (!user_data) {
    return;
  }
  DisplayContext *pctx = (DisplayContext*)user_data;
  pctx->mouse_x = x;
  pctx->mouse_y = y;
  UpdateDepthImage(*pctx);
  display_ShowImage(1);
}

//find related window context
static DisplayContext *pick_context(const char *window_name) {
  std::string name(window_name);
  auto it = data.find(name);
  DisplayContext *pctx = nullptr;
  if (it != data.end()) {
    pctx = it->second.get();
  } else { //when not found , create new item
    if (data.size() >= MAX_WINDOW_NUM) {//reach max window number ,give up
      return nullptr;
    }
    data.insert(std::pair<std::string, std::shared_ptr<DisplayContext>>(name, std::make_shared<DisplayContext>(name)));
    it = data.find(name);
    pctx = it->second.get();
  }
  return pctx;
}


//export function////////////////////////////////////////////////

void  display_Init(void) {
  cv::destroyAllWindows();
  data.clear();
}

TBool display_SetRawImage(char *szWindowName, TU8 *pBuf, TU16 nWidth, TU16 nHeight) {
  if (!szWindowName || !pBuf || nWidth == 0 || nHeight == 0) return TFalse;
  DisplayContext *pctx = pick_context(szWindowName);
  if (!pctx) {
    return TFalse;
  }
  pctx->image.create(nHeight, nWidth, CV_8UC1);
  memcpy(pctx->image.data, pBuf, nWidth * nHeight);
  return TTrue;
}

TBool display_SetDepthImage(char *szWindowName, TU16 *pDepth, TU16 nDepthSize, float fFov) {
  if (!szWindowName) return TFalse;
  DisplayContext *pctx = pick_context(szWindowName);
  if (!pctx) {//error
    return TFalse;
  }
  if (pDepth && nDepthSize > 0) {
    pctx->depth_data.resize(nDepthSize);
    memcpy(&pctx->depth_data[0], pDepth, nDepthSize * sizeof(uint16_t));
  } else {
    pctx->depth_data.resize(1);
    pctx->depth_data[0] = 0;
  }
  pctx->depth_fov = fFov;
  UpdateDepthImage(*pctx);
  return TTrue;
}

void  display_ShowImage(TU32 nTimeout) {
  for (ContextMap::iterator it = data.begin(); it != data.end(); it++) {
    if (!it->second.get()->image.empty()) {
      cv::imshow(it->first, it->second.get()->image);
    }
  }

  int ret = cv::waitKey(nTimeout);
  if (ret >= 0) {
    g_nLastKey = (TU32)ret;
  }
}

TU32  display_GetEvent(void) {
  TU32 nKey = g_nLastKey;
  g_nLastKey = 0;

  return nKey;
}

void  display_SetStatInfo(char *szWindowName, TU32 nFrameCount, float fFps) {
  if (!szWindowName) return;
  DisplayContext *pctx = pick_context(szWindowName);
  if (!pctx) {
    return;
  }
  pctx->show_extra_info = true;
  pctx->frm_count = nFrameCount;
  pctx->fps = fFps;
  UpdateDepthImage(*pctx);
}

void  display_SetDebugImageInfo(char *szWindowName, TU8 nProgress, const char *szFileName) {
  if (!szWindowName) return;
  DisplayContext *pctx = pick_context(szWindowName);
  if (!pctx) {
    return;
  }
  pctx->show_extra_info = true;
  pctx->dbg_progress = nProgress;
  pctx->dbg_filename = szFileName;
  UpdateDepthImage(*pctx);
}

void  display_SetFovDeviation(char *szWindowName, float fov_deviation) {
  if (!szWindowName) return;
  DisplayContext *pctx = pick_context(szWindowName);
  if (!pctx) {
    return;
  }
  pctx->fov_deviation = fov_deviation;
  UpdateDepthImage(*pctx);
}

void display_SaveCurrentImage(char *szWindowName, char *szFileName) {
  if (!szWindowName || !szFileName) return;
  DisplayContext *pctx = pick_context(szWindowName);
  if (!pctx) {
    return;
  }
  if (pctx->image.empty()){
    return;
  }
  cv::imwrite(szFileName, pctx->image);
}
