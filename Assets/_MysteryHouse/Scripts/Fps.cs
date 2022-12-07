using System;
using System.Collections;
using TMPro;
using UnityEngine;

namespace _BioMinds.Scripts
{
	public class Fps : MonoBehaviour {

		public TextMeshProUGUI theText;

		private float _count;

        private void Start() {
			
			StartCoroutine(StartCounterCor());
		}

        private IEnumerator StartCounterCor() {
			while (true) {
				if (Math.Abs(Time.timeScale - 1) < 0.01) {
                    yield return new WaitForSeconds(0.1f);
                    _count = Mathf.Round(1 / Time.deltaTime);
					theText.text = $"FPS {_count:0.0}";
				} else {
					theText.text = "Pause";
				}
				yield return new WaitForSeconds(0.1f);
			}
		}


	}
}
