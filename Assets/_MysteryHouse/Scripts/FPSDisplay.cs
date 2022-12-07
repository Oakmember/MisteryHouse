using TMPro;
using UnityEngine;

namespace _BioMinds.Scripts
{
	public class FPSDisplay : MonoBehaviour {

		public TextMeshProUGUI theText;

		private float _deltaTime;

		private void Update() {
			_deltaTime += (Time.unscaledDeltaTime - _deltaTime) * 0.1f;

			var mSec = _deltaTime * 1000.0f;
			var fps = 1.0f / _deltaTime;
			theText.text =   $"{mSec:0.0} ms ({fps:0.} fps)";
		}
	}
}

	
