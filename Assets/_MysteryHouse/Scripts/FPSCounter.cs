using TMPro;
using UnityEngine;
using UnityEngine.Serialization;

namespace _BioMinds.Scripts
{
	public class FPSCounter : MonoBehaviour {

		public float updateInterval = 0.5f;

		private float _deltaFps = 0f; // FPS accumulated over the interval
		private int _frames = 0; // Frames drawn over the interval
		private float _timeLeft; // Left time for current interval

		public TextMeshProUGUI theText;
		[FormerlySerializedAs("FPS")] public float fps;

		private void Start() {
			_timeLeft = updateInterval;
		}

		private void Update() {
			_timeLeft -= Time.deltaTime;
			_deltaFps += Time.timeScale / Time.deltaTime;
			++_frames;

			// Interval ended - update GUI text and start new interval
			if (!(_timeLeft <= 0f)) return;
			// display two fractional digits (f2 format)
			fps = _deltaFps / _frames;
			theText.text = $"{fps:f2} FPS";
			if ((_deltaFps / _frames) < 1) {
				theText.text = "";
			}
			_timeLeft = updateInterval;
			_deltaFps = 0f;
			_frames = 0;
		}
	}
}
