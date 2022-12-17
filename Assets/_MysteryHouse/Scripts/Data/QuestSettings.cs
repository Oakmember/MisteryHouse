using Shared;
using UnityEngine;

namespace _BioMinds.Scripts.Data {

    [CreateAssetMenu(fileName = "QuestSettings", menuName = "Settings/QuestSettings")]
    public class QuestSettings : ScriptableObject {

        public QuestType ButterflyType = QuestType.None;
        public bool isCompleted = false;
    }
}
