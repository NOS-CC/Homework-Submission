import cv2
import mediapipe as mp

# 初始化 MediaPipe 手部检测模块
mp_hands = mp.solutions.hands
mp_drawing = mp.solutions.drawing_utils

# 配置 MediaPipe Hands 模型
hands = mp_hands.Hands(
    static_image_mode=False,
    max_num_hands=2,
    model_complexity=1,
    min_detection_confidence=0.5,
    min_tracking_confidence=0.5
)

# === 请修改此处为你自己的手势视频路径 ===
video_path = "input.mp4" 

cap = cv2.VideoCapture(video_path)

if not cap.isOpened():
    print(f"错误：无法打开视频文件 {video_path}，请检查路径和文件名是否正确。")
    exit()

# 创建一个可自由缩放的窗口
window_name = 'MediaPipe Hand Gesture Recognition'
cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)

# 可选：如果你希望一开始就以一个固定较小的尺寸显示（例如宽 960，高 540），可以解开下面这行注释：
# cv2.resizeWindow(window_name, 960, 540)

print("视频已成功加载，正在处理中... 窗口支持自由缩放/最大化，按键盘上的 'q' 键可退出。")

while cap.isOpened():
    success, frame = cap.read()
    if not success:
        print("视频播放完毕或读取结束。")
        break

    # 1. 将 OpenCV 的 BGR 图像转换为 RGB（MediaPipe 需要 RGB 输入）
    image_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    
    # 2. 禁用图像写入以提升处理性能
    image_rgb.flags.writeable = False
    
    # 3. 执行手势与关键点推理
    results = hands.process(image_rgb)
    
    # 4. 恢复图像写入属性，准备绘制
    image_rgb.flags.writeable = True
    image = cv2.cvtColor(image_rgb, cv2.COLOR_RGB2BGR)

    # 5. 判断是否成功识别到手势
    if results.multi_hand_landmarks:
        print(f"[成功识别] 当前帧检测到 {len(results.multi_hand_landmarks)} 只手")
        
        for idx, hand_landmarks in enumerate(results.multi_hand_landmarks):
            # 在视频画面的手部上绘制 21 个骨骼关键点连线
            mp_drawing.draw_landmarks(
                image, 
                hand_landmarks, 
                mp_hands.HAND_CONNECTIONS,
                mp_drawing.DrawingSpec(color=(0, 255, 0), thickness=2, circle_radius=2),
                mp_drawing.DrawingSpec(color=(0, 0, 255), thickness=2, circle_radius=2)
            )
            
            # 获取手腕（第0号节点）的坐标在画面上作文本输出
            wrist = hand_landmarks.landmark[mp_hands.HandLandmark.WRIST]
            h, w, _ = image.shape
            cx, cy = int(wrist.x * w), int(wrist.y * h)
            
            cv2.putText(image, f"Hand {idx+1} Detected!", (cx - 40, cy - 20),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2, cv2.LINE_AA)
    else:
        cv2.putText(image, "Searching for hands...", (30, 50),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 255), 2, cv2.LINE_AA)

    # 6. 展示视频
    cv2.imshow(window_name, image)

    # 按下 'q' 键退出
    if cv2.waitKey(25) & 0xFF == ord('q'):
        break

# 释放资源
cap.release()
cv2.destroyAllWindows()
hands.close()
