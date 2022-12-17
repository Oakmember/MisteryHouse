using Shared;
using System.Collections.Generic;
using UnityEngine;

namespace _BioMinds.Scripts.Data {

    [CreateAssetMenu(fileName = "SafeTurnLeverSettings", menuName = "Settings/SafeTurnLeverSettings")]
    public class SafeTurnLeverSettings : ScriptableObject {

        [SerializeField]
        private List<int> codeValues = new List<int>();

        [SerializeField]
        private AudioClip foundCodeSound = null;

        public List<int> CodeValues => codeValues;

        public AudioClip FoundCodeSound => foundCodeSound;

    }
}
