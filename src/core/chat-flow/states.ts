import moment from "moment";
import { compact, noop } from "lodash";
import {
  onButtonPressed,
  onButtonReleased,
  onButtonDoubleClick,
  display,
  getCurrentStatus,
  onCameraCapture,
  onTextInput,
  isButtonDown,
} from "../../device/display";
import {
  recordAudio,
  recordAudioManually,
  recordFileFormat,
  getDynamicVoiceDetectLevel,
} from "../../device/audio";
import { chatWithLLMStream } from "../../cloud-api/server";
import { isImMode } from "../../cloud-api/llm";
import { getSystemPromptWithKnowledge } from "../Knowledge";
import { enableRAG } from "../../cloud-api/knowledge";
import { getSystemPrompt } from "../../config/llm-config";
import { getOpenAIClient, getOpenAILLMModel } from "../../cloud-api/openai/openai";
import { cameraDir } from "../../utils/dir";
import {
  clearPendingCapturedImgForChat,
  getLatestGenImg,
  getLatestDisplayImg,
  getLatestShowedImage,
  setLatestCapturedImg,
  setPendingCapturedImgForChat,
} from "../../utils/image";
import { sendWhisplayIMMessage } from "../../cloud-api/whisplay-im/whisplay-im";
import { ChatFlowContext, FlowName, FlowStateHandler } from "./types";
import {
  enterCameraMode,
  handleCameraModePress,
  handleCameraModeRelease,
  onCameraModeExit,
  resetCameraModeControl,
} from "./camera-mode";
import { DEFAULT_EMOJI } from "../../utils";
import { isMusicPlaying, getCurrentTrackTitle, stopMusicPlayback, startPendingMusicPlayback, onMusicTrackChange, onMusicPlaybackEnd } from "../../device/music-player";
import { autoSaveExchange } from "../../config/mempalace";
import { STATE_EMOJIS } from "../../config/state-emojis";
import { llmFuncMap } from "../../config/llm-tools";
import {
  SETTINGS_OPEN_GRACE_MS,
  SETTINGS_SELECT_HOLD_MS,
} from "./settings-menu";
import { ToolReturnTag } from "../../type";
import { setLatestVisionAnalysis } from "../../utils/vision-analysis";

const imageIntentPatterns = [
  /\bwhat do you see\b/i,
  /\bdescribe (this|the) (image|photo|picture)\b/i,
  /\banaly[sz]e (this|the) (image|photo|picture)\b/i,
  /\bwhat(?:'s| is) in (this|the) (image|photo|picture)\b/i,
  /\bdo you see\b/i,
  /\bread (the )?text\b/i,
  /\bocr\b/i,
];

function shouldRouteToVision(prompt: string): boolean {
  const trimmed = prompt.trim();
  if (!trimmed || !getLatestShowedImage()) {
    return false;
  }
  return imageIntentPatterns.some((pattern) => pattern.test(trimmed));
}

function stripToolTag(result: string): string {
  return result.replace(/^\[(success|error|response)\]\s*/i, "").trim();
}

async function streamVisionRelayReply(
  userPrompt: string,
  visionAnalysis: string,
  onChunk: (chunk: string) => void,
): Promise<string> {
  const openai = getOpenAIClient();
  if (!openai) {
    return "";
  }
  const stream = await openai.chat.completions.create({
    model: getOpenAILLMModel(),
    stream: true,
    messages: [
      {
        role: "system",
        content:
          `${getSystemPrompt()}\n` +
          "You are answering a question about an already analyzed image. " +
          "Use the supplied vision analysis as your only visual context. " +
          "Reply naturally in your active personality, stay concise, and do not mention Gemini, tools, or backend analysis unless the user asks.",
      },
      {
        role: "user",
        content:
          `User question: ${userPrompt}\n\n` +
          `Vision analysis:\n${visionAnalysis}\n\n` +
          "Answer the user's question naturally. If the analysis seems uncertain, briefly say so.",
      },
    ],
  });
  let answer = "";
  for await (const chunk of stream) {
    const text = chunk.choices[0]?.delta?.content || "";
    if (!text) {
      continue;
    }
    answer += text;
    onChunk(text);
  }
  return answer.trim();
}

export const flowStates: Record<FlowName, FlowStateHandler> = {
  sleep: (ctx: ChatFlowContext) => {
    onButtonDoubleClick(() => {
      if (ctx.hasLastAnswer()) {
        ctx.replayLastAnswer();
        return;
      }
      if (ctx.enableCamera) {
        const captureImgPath = `${cameraDir}/capture-${moment().format(
          "YYYYMMDD-HHmmss",
        )}.jpg`;
        enterCameraMode(captureImgPath);
        ctx.transitionTo("camera");
      }
    });
    onButtonPressed(() => {
      resetCameraModeControl();
      // Stop any playing music when waking up
      stopMusicPlayback();
      ctx.transitionTo("listening");
    });
    onButtonReleased(noop);
    onCameraModeExit(null);
    onTextInput((text: string) => {
      if (ctx.currentFlowName !== "sleep") return;
      ctx.answerId += 1;
      ctx.asrText = text;
      display({ status: "recognizing", text, text_input_enabled: false });
      ctx.transitionTo("answer");
    });
    display({
      status: "idle",
      emoji: STATE_EMOJIS.idle,
      RGB: "#000055",
      rag_icon_visible: false,
      ...(getCurrentStatus().text.endsWith("Listening...") || !getCurrentStatus().text
        ? {
          text: `Long press to talk${ctx.hasLastAnswer() ? ",\ndouble press to replay" : ctx.enableCamera ? ",\ndouble press for camera" : ""
            }.`,
        }
        : {}),
    });
  },
  camera: (ctx: ChatFlowContext) => {
    onButtonDoubleClick(null);
    onButtonPressed(() => {
      handleCameraModePress();
    });
    onButtonReleased(() => {
      handleCameraModeRelease();
    });
    onCameraCapture(() => {
      const captureImagePath = getCurrentStatus().capture_image_path;
      if (!captureImagePath) {
        return;
      }
      setLatestCapturedImg(captureImagePath);
      setPendingCapturedImgForChat(captureImagePath);
      display({ image_icon_visible: true });
    });
    onCameraModeExit(() => {
      if (ctx.currentFlowName === "camera") {
        ctx.transitionTo("sleep");
      }
    });
    display({
      status: "camera",
      emoji: STATE_EMOJIS.camera,
      RGB: "#00ff88",
    });
  },
  music: (ctx: ChatFlowContext) => {
    // Start deferred music playback when entering music state
    startPendingMusicPlayback();

    // Update display when track changes during continuous playback
    onMusicTrackChange((title) => {
      if (ctx.currentFlowName === "music") {
        display({ text: `Now playing: ${title}` });
      }
    });

    // Return to sleep when non-continuous playback finishes
    onMusicPlaybackEnd(() => {
      if (ctx.currentFlowName === "music") {
        onMusicTrackChange(null);
        onMusicPlaybackEnd(null);
        ctx.transitionTo("sleep");
      }
    });

    onButtonDoubleClick(null);
    onButtonPressed(() => {
      // Stop music immediately when button is pressed
      onMusicTrackChange(null);
      onMusicPlaybackEnd(null);
      stopMusicPlayback();
      ctx.transitionTo("listening");
    });
    onButtonReleased(noop);

    const trackTitle = getCurrentTrackTitle();
    display({
      status: "music",
      emoji: STATE_EMOJIS.music,
      RGB: "#0066aa",
      text:
        ctx.musicDisplayText ||
        (isMusicPlaying() && trackTitle
          ? `Now playing: ${trackTitle}`
          : "Music mode. Press the button to talk."),
      rag_icon_visible: false,
    });
  },
  listening: (ctx: ChatFlowContext) => {
    ctx.enterMusicAfterAnswer = false;
    ctx.musicDisplayText = "";
    ctx.isFromWakeListening = false;
    ctx.answerId += 1;
    ctx.wakeSessionActive = false;
    ctx.endAfterAnswer = false;
    onButtonDoubleClick(null);
    ctx.currentRecordFilePath = `${ctx.recordingsDir
      }/user-${Date.now()}.${recordFileFormat}`;
    onButtonPressed(noop);
    const listeningStartedAt = Date.now();
    const manualRecordMaxMs = ctx.getManualRecordMaxSec() * 1000;
    let recordLimitReached = false;
    let waitingForReleaseAfterLimit = false;
    let recordingCompleted = false;
    let released = false;
    let settingsTriggered = false;
    let recordLimitTimer: NodeJS.Timeout | null = null;
    let settingsTimer: NodeJS.Timeout | null = null;
    // If button was already released before we entered this state, go back to sleep
    if (!isButtonDown()) {
      console.log("[listening] Button already released, returning to sleep");
      ctx.transitionTo("sleep");
      return;
    }
    const { result, stop } = recordAudioManually(ctx.currentRecordFilePath);
    recordLimitTimer = setTimeout(() => {
      if (ctx.currentFlowName !== "listening") {
        return;
      }
      recordLimitReached = true;
      waitingForReleaseAfterLimit = true;
      stop();
      display({
        status: "listening",
        emoji: STATE_EMOJIS.listening,
        RGB: "#6048ff",
        text: `Release to send.\nKeep holding ${SETTINGS_OPEN_GRACE_MS / 1000}s for settings...`,
        rag_icon_visible: false,
      });
      settingsTimer = setTimeout(() => {
        if (ctx.currentFlowName !== "listening" || released) {
          return;
        }
        settingsTriggered = true;
        ctx.openSettingsMenu(true);
      }, SETTINGS_OPEN_GRACE_MS);
    }, manualRecordMaxMs);

    const handleRelease = () => {
      released = true;
      if (recordLimitTimer) {
        clearTimeout(recordLimitTimer);
        recordLimitTimer = null;
      }
      if (settingsTimer) {
        clearTimeout(settingsTimer);
        settingsTimer = null;
      }
      if (Date.now() - listeningStartedAt < 500) {
        // Too short to be meaningful — stop recording and return to sleep
        console.log("[listening] Button released too quickly, returning to sleep");
        stop();
        ctx.transitionTo("sleep");
        return;
      }
      if (settingsTriggered) {
        return;
      }
      if (recordLimitReached) {
        waitingForReleaseAfterLimit = false;
        if (recordingCompleted && ctx.currentFlowName === "listening") {
          ctx.transitionTo("asr");
        }
        return;
      }
      stop();
      display({
        RGB: "#ff6800",
        image: "",
      });
    };
    onButtonReleased(handleRelease);
    result
      .then(() => {
        recordingCompleted = true;
        if (settingsTriggered || ctx.currentFlowName !== "listening") {
          return;
        }
        if (waitingForReleaseAfterLimit && !released) {
          return;
        }
        ctx.transitionTo("asr");
      })
      .catch((err) => {
        console.error("Error during recording:", err);
        ctx.transitionTo("sleep");
      });
    display({
      status: "listening",
      emoji: STATE_EMOJIS.listening,
      RGB: "#00ff00",
      text: "Listening...",
      rag_icon_visible: false,
    });
  },
  wake_listening: (ctx: ChatFlowContext) => {
    ctx.enterMusicAfterAnswer = false;
    ctx.musicDisplayText = "";
    ctx.isFromWakeListening = true;
    ctx.answerId += 1;
    ctx.currentRecordFilePath = `${ctx.recordingsDir
      }/user-${Date.now()}.${recordFileFormat}`;
    onButtonPressed(() => {
      ctx.transitionTo("listening");
    });
    onButtonReleased(noop);
    display({
      status: "detecting",
      emoji: STATE_EMOJIS.recognizing,
      RGB: "#00ff00",
      text: "Detecting voice level...",
      rag_icon_visible: false,
    });
    getDynamicVoiceDetectLevel().then((level) => {
      display({
        status: "listening",
        emoji: STATE_EMOJIS.listening,
        RGB: "#00ff00",
        text: `(Detect level: ${level}%) Listening...`,
        rag_icon_visible: false,
      });
      recordAudio(ctx.currentRecordFilePath, ctx.wakeRecordMaxSec, level)
        .then(() => {
          ctx.transitionTo("asr");
        })
        .catch((err) => {
          console.error("Error during auto recording:", err);
          ctx.endWakeSession();
          ctx.transitionTo("sleep");
        });
    });
  },
  asr: (ctx: ChatFlowContext) => {
    display({
      status: "recognizing",
      emoji: STATE_EMOJIS.recognizing,
    });
    onButtonDoubleClick(null);
    Promise.race([
      ctx.recognizeAudio(ctx.currentRecordFilePath, ctx.isFromWakeListening),
      new Promise<string>((resolve) => {
        onButtonPressed(() => {
          resolve("[UserPress]");
        });
        onButtonReleased(noop);
      }),
    ]).then((result) => {
      if (ctx.currentFlowName !== "asr") return;
      if (result === "[UserPress]") {
        ctx.transitionTo("listening");
        return;
      }
      if (result) {
        console.log("Audio recognized result:", result);
        if (ctx.shouldOpenSettingsMenu(result)) {
          ctx.openSettingsMenu();
          return;
        }
        ctx.asrText = result;
        ctx.endAfterAnswer = ctx.shouldEndAfterAnswer(result);
        if (ctx.wakeSessionActive) {
          ctx.wakeSessionLastSpeechAt = Date.now();
        }
        display({ status: "recognizing", text: result });
        ctx.transitionTo("answer");
        return;
      }
      if (ctx.wakeSessionActive) {
        if (ctx.shouldContinueWakeSession()) {
          ctx.transitionTo("wake_listening");
        } else {
          ctx.endWakeSession();
          ctx.transitionTo("sleep");
        }
        return;
      }
      ctx.transitionTo("sleep");
    });
  },
  settings: (ctx: ChatFlowContext) => {
    onButtonDoubleClick(null);
    let selectTimer: NodeJS.Timeout | null = null;
    let holdTriggered = false;
    onButtonPressed(() => {
      holdTriggered = false;
      selectTimer = setTimeout(() => {
        if (ctx.currentFlowName !== "settings") {
          return;
        }
        holdTriggered = true;
        ctx.activateSettingsSelection();
      }, SETTINGS_SELECT_HOLD_MS);
    });
    onButtonReleased(() => {
      if (selectTimer) {
        clearTimeout(selectTimer);
        selectTimer = null;
      }
      if (ctx.consumeSettingsReleaseGuard()) {
        return;
      }
      if (ctx.currentFlowName !== "settings") {
        return;
      }
      if (holdTriggered) {
        holdTriggered = false;
        return;
      }
      ctx.moveSettingsSelection();
    });
    ctx.renderSettingsMenu();
  },
  answer: (ctx: ChatFlowContext) => {
    ctx.enterMusicAfterAnswer = false;
    ctx.musicDisplayText = "";
    display({
      status: "answering...",
      emoji: STATE_EMOJIS.answering,
      RGB: "#00c8a3",
    });
    const currentAnswerId = ctx.answerId;
    if (isImMode) {
      const prompt: {
        role: "system" | "user";
        content: string;
      }[] = [
          {
            role: "user",
            content: ctx.asrText,
          },
        ];
      sendWhisplayIMMessage(prompt)
        .then((ok) => {
            if (ok) {
            ctx.rememberLastAnswer({
              text: ctx.pendingExternalReply || ctx.asrText,
              emoji: STATE_EMOJIS.externalIdle,
            });
            display({
              status: "idle",
              emoji: STATE_EMOJIS.externalIdle,
              RGB: "#000055",
              image_icon_visible: false,
            });
          } else {
            display({
              status: "error",
              emoji: STATE_EMOJIS.error,
              text: "OpenClaw send failed",
              image_icon_visible: false,
            });
          }
        })
        .finally(() => {
          clearPendingCapturedImgForChat();
          ctx.transitionTo("sleep");
        });
      return;
    }
    onButtonPressed(() => {
      ctx.transitionTo("listening");
    });
    onButtonReleased(noop);
    const {
      partial,
      endPartial,
      getPlayEndPromise,
      stop: stopPlaying,
    } = ctx.streamResponser;
    let llmResponseText = "";
    const trackingPartial = (text: string): void => {
      llmResponseText += text;
      if (currentAnswerId === ctx.answerId) partial(text);
    };
    ctx.partialThinking = "";
    ctx.thinkingSentences = [];
    [() => Promise.resolve().then(() => ""), getSystemPromptWithKnowledge]
    [enableRAG ? 1 : 0](ctx.asrText)
      .then((res: string) => {
        if (shouldRouteToVision(ctx.asrText) && typeof llmFuncMap.describeImage === "function") {
          display({
            text: "[describeImage]Analyzing uploaded image...",
          });
          llmFuncMap.describeImage({ prompt: ctx.asrText })
            .then(async (result) => {
              if (currentAnswerId !== ctx.answerId) {
                return;
              }
              const cleaned = stripToolTag(result);
              if (!cleaned) {
                trackingPartial("I couldn't analyze that image.");
                endPartial();
                return;
              }
              if (result.startsWith(ToolReturnTag.Error)) {
                setLatestVisionAnalysis({
                  question: ctx.asrText,
                  rawResponse: cleaned,
                  relayResponse: cleaned,
                  updatedAt: Date.now(),
                  ok: false,
                });
                trackingPartial(cleaned);
                endPartial();
                return;
              }
              const relayReply = await streamVisionRelayReply(
                ctx.asrText,
                cleaned,
                (chunk) => {
                  if (currentAnswerId === ctx.answerId) {
                    trackingPartial(chunk);
                  }
                },
              );
              const finalReply = relayReply || cleaned || "I couldn't analyze that image.";
              if (!relayReply && currentAnswerId === ctx.answerId) {
                trackingPartial(finalReply);
              }
              setLatestVisionAnalysis({
                question: ctx.asrText,
                rawResponse: cleaned,
                relayResponse: finalReply,
                updatedAt: Date.now(),
                ok: true,
              });
              endPartial();
            })
            .catch((error) => {
              console.error("Vision routing failed:", error);
              if (currentAnswerId !== ctx.answerId) {
                return;
              }
              const message = "I couldn't analyze that image right now.";
              setLatestVisionAnalysis({
                question: ctx.asrText,
                rawResponse: message,
                relayResponse: message,
                updatedAt: Date.now(),
                ok: false,
              });
              trackingPartial(message);
              endPartial();
            });
          return;
        }
        let knowledgePrompt = res;
        if (res) {
          console.log("Retrieved knowledge for RAG:\n", res);
        }
        if (ctx.knowledgePrompts.includes(res)) {
          console.log(
            "[RAG] Knowledge prompt already used in this session, skipping to avoid repetition.",
          );
          knowledgePrompt = "";
        }
        if (knowledgePrompt) {
          ctx.knowledgePrompts.push(knowledgePrompt);
        }
        display({
          rag_icon_visible: Boolean(enableRAG && knowledgePrompt),
        });
        const prompt: {
          role: "system" | "user";
          content: string;
        }[] = compact([
          knowledgePrompt
            ? {
              role: "system",
              content: knowledgePrompt,
            }
            : null,
          {
            role: "user",
            content: ctx.asrText,
          },
        ]);
        chatWithLLMStream(
          prompt,
          (text) => { if (currentAnswerId === ctx.answerId) trackingPartial(text); },
          () => currentAnswerId === ctx.answerId && endPartial(),
          (partialThinking) =>
            currentAnswerId === ctx.answerId &&
            ctx.partialThinkingCallback(partialThinking),
          (functionName: string, result?: string) => {
            if (
              functionName === "endConversation" &&
              result?.startsWith("[success]")
            ) {
              ctx.endAfterAnswer = true;
            }
            if (
              functionName === "generateImage" &&
              result?.startsWith("[success]")
            ) {
              const img = getLatestGenImg();
              if (img) {
                display({ image: img });
              }
            }
            if (
              functionName.startsWith("playMusic") &&
              result?.startsWith("[success]")
            ) {
              ctx.enterMusicAfterAnswer = true;
              ctx.musicDisplayText = result.replace(/^\[success\]/, "").trim();
            }
            if (result) {
              display({
                text: `[${functionName}]${result}`,
              });
            } else {
              display({
                text: `Invoking [${functionName}]... {count}s`,
              });
            }
          },
        );
      });
    getPlayEndPromise().then(() => {
      if (ctx.currentFlowName === "answer") {
        const img = getLatestDisplayImg();
        ctx.rememberLastAnswer({
          text: llmResponseText,
          emoji: STATE_EMOJIS.answering,
          image: img || "",
        });
        autoSaveExchange(ctx.asrText, llmResponseText);
        clearPendingCapturedImgForChat();
        display({ image_icon_visible: false });
        if (ctx.wakeSessionActive || ctx.endAfterAnswer) {
          if (ctx.endAfterAnswer) {
            ctx.endWakeSession();
            ctx.transitionTo("sleep");
          } else {
            ctx.transitionTo("wake_listening");
          }
          return;
        }
        if (ctx.enterMusicAfterAnswer) {
          ctx.transitionTo("music");
          return;
        }
        if (img) {
          ctx.transitionTo("image");
        } else {
          ctx.transitionTo("sleep");
        }
      }
    });
    onButtonPressed(() => {
      stopPlaying();
      clearPendingCapturedImgForChat();
      display({ image_icon_visible: false });
      ctx.transitionTo("listening");
    });
    onButtonReleased(noop);
  },
  image: (ctx: ChatFlowContext) => {
    onButtonPressed(() => {
      display({ image: "" });
      ctx.transitionTo("listening");
    });
    onButtonReleased(noop);
  },
  external_answer: (ctx: ChatFlowContext) => {
    if (!ctx.pendingExternalReply && !ctx.pendingExternalImageUrl) {
      ctx.transitionTo("sleep");
      return;
    }
    display({
      status: "answering...",
      RGB: "#00c8a3",
      emoji: ctx.pendingExternalEmoji || STATE_EMOJIS.answering,
    });
    onButtonPressed(() => {
      ctx.streamResponser.stop();
      display({ image: "" });
      ctx.transitionTo("listening");
    });
    onButtonReleased(noop);
    const replyText = ctx.pendingExternalReply;
    const replyEmoji = ctx.pendingExternalEmoji;
    const replyImageUrl = ctx.pendingExternalImageUrl;
    ctx.currentExternalEmoji = replyEmoji;
    ctx.pendingExternalReply = "";
    ctx.pendingExternalEmoji = "";
    ctx.pendingExternalImageUrl = "";

    // Display the image if one was provided
    if (replyImageUrl) {
      display({ image: replyImageUrl });
    }

    if (replyText) {
      void ctx.streamExternalReply(replyText, replyEmoji);
      ctx.streamResponser.getPlayEndPromise().then(() => {
        if (ctx.currentFlowName !== "external_answer") return;
        ctx.rememberLastAnswer({
          text: replyText,
          emoji: replyEmoji || STATE_EMOJIS.answering,
          image: replyImageUrl || "",
        });
        if (ctx.wakeSessionActive || ctx.endAfterAnswer) {
          if (ctx.endAfterAnswer) {
            ctx.endWakeSession();
            ctx.transitionTo("sleep");
          } else {
            ctx.transitionTo("wake_listening");
          }
        } else if (replyImageUrl) {
          // Stay in image display mode after TTS finishes
          ctx.transitionTo("image");
        } else {
          ctx.transitionTo("sleep");
        }
      });
    } else {
      // Image only, no text to speak — go to image display mode
      ctx.transitionTo("image");
    }
  },
};
